import sys
import os
import logging
import getopt
import logging.handlers
import timeit

thisfile = os.path.abspath(os.path.normpath(__file__))
pythonlib = os.path.join(os.path.dirname(os.path.dirname(thisfile)), "pythonlib")
genpy = os.path.join(os.path.dirname(thisfile), "gen-py.twisted")
sys.path.insert(0, pythonlib)
sys.path.insert(0, genpy)

from twisted.internet import threads, defer
from twisted.internet.protocol import ClientCreator
from twisted.python import log, failure
from twisted.internet.error import ConnectionRefusedError

from thrift.transport import TTwisted
from thrift.protocol import TBinaryProtocol

from imaging import Imaging
import imaging.constants as imaging_constants

def sleep_deferred(reactor, t):
    d = defer.Deferred()
    reactor.callLater(t, d.callback, None)
    d.addCallback(lambda x: None)
    return d


def formatFileSize(maxmem):
    if maxmem > 1024 * 1024 * 1024:
        maxmem = "%.2f" % (float(maxmem) / (1024.0*1024.0*1024.0)) + " Gb"
    elif maxmem > 1024 * 1024:
        maxmem = "%.2f" % (float(maxmem) / (1024.0*1024.0)) + " Mb"
    else:
        maxmem = "%.2f" % (float(maxmem) / (1024.0)) + " Kb"
    return maxmem


class Worker(object):
    
    def __init__(self, reactor):
        self.reactor = reactor


    @defer.inlineCallbacks
    def loop(self, args):
        try:
            yield self.connect()
            
            yield self.doStuff(args)
        except (KeyboardInterrupt, SystemExit):
            pass
        except imaging_constants.InvalidOperation, e:
            logging.error("imaging error: %s (%d)", e.why, e.what, exc_info=True)
        except Exception:
            logging.error("error running loop", exc_info=True)
        
        self.reactor.callLater(0, self.reactor.stop)


    @defer.inlineCallbacks
    def connect(self):
        pfactory = TBinaryProtocol.TBinaryProtocolFactory()
        worker_client = ClientCreator(self.reactor, TTwisted.ThriftClientProtocol,
                                    client_class=Imaging.Client, iprot_factory=pfactory)

        while 1:
            try:        
                self.worker_connect = yield worker_client.connectTCP("localhost", 9090)
                break
            except ConnectionRefusedError:        
                logging.warning("could not connect, will retry in 1s")
    
                yield sleep_deferred(self.reactor, 1)
            

    @defer.inlineCallbacks
    def doStuff(self, args):
        if (args and args[0] == "simple") or not args:
            jpeg = yield self.worker_connect.client.mandelbrot(200, 200)
            file(r"J:\mandelbrot.jpg", "wb").write(jpeg)
            
            jpegccw = yield self.worker_connect.client.transform(imaging_constants.Transform.ROTATE90CCW, jpeg)
            file(r"J:\mandelbrot_ccw.jpg", "wb").write(jpegccw)
            defer.returnValue(None)
        
        if args and args[0] == "stress":
            yield self.doStress(args[1:])
            defer.returnValue(None)
        
        raise Exception("Unknown command '%s'" % args[0])
        

    @defer.inlineCallbacks
    def __doStressOne(self, res, cnt, dims):
        if isinstance(res, failure.Failure):
            defer.returnValue(res)
        
        worker_connect = res
        logging.debug("start stress #%d", cnt)
        jpeg = yield worker_connect.client.mandelbrot(dims, dims)
        jpegccw = yield worker_connect.client.transform(imaging_constants.Transform.ROTATE90CCW, jpeg)
        logging.debug("end stress #%d", cnt)

        defer.returnValue(len(jpegccw))


    @defer.inlineCallbacks
    def doStress(self, args):
        start = timeit.default_timer()
        numiter = 100
        dims = 200
        logging.info("starting stress test")
        ds = []    
        pfactory = TBinaryProtocol.TBinaryProtocolFactory()
        for cnt in range(numiter):
            worker_client = ClientCreator(self.reactor, TTwisted.ThriftClientProtocol,
                                        client_class=Imaging.Client, iprot_factory=pfactory)
    
            d = worker_client.connectTCP("127.0.0.1", 9090)
            ds.append(d)
            
            d.addBoth(self.__doStressOne, cnt, dims)

        totalsize = yield defer.DeferredList(ds, fireOnOneErrback=True)
        totalsize = map(lambda x: x[1], totalsize)
        totalsize = reduce(lambda x,y: x+y, totalsize)
        totalsize *= 3
        elapsed = timeit.default_timer() - start
        logging.info("stress dims=%dx%d test #=%d elapsed=%.2fs trans=%.2f/s band=%s/s", dims, dims, numiter,
                     elapsed, numiter / elapsed, formatFileSize(totalsize / elapsed))
        
        
 
def main():
    def usage():
        theusage = (
            "Usage: python pyclient.py <options>",
            "\nOptional:",
            "  -h : display this help page",
            "  -v : increase logging",
            "",
            )

        sys.stderr.write(string.join(theusage, '\n'))
        sys.stderr.write('\n')

    try:
        if len(sys.argv) <= 0:
            usage()
            return - 1

        longoptions = ()
        opts, args = getopt.getopt(sys.argv[1:], 'hv', longoptions)
        loggingLevel = logging.WARNING
        for o, a in opts:
            if o == '-h':
                usage()
                return 0
            elif o == "-v":
                if loggingLevel == logging.WARNING:
                    loggingLevel = logging.INFO
                else:
                    loggingLevel = logging.DEBUG

    except getopt.GetoptError:
        usage()
        return - 1

    # set up the logging
    logger = logging.getLogger()
    formatter = logging.Formatter('%(asctime)s [%(module)s, %(funcName)s, L%(lineno)d] %(levelname)s, %(message)s')

    logger.setLevel(loggingLevel)
    
    hdlr = logging.StreamHandler(sys.stderr)
    hdlr.setFormatter(formatter)
    logger.addHandler(hdlr)
    
    hdlr = logging.handlers.RotatingFileHandler(os.path.join(os.path.dirname(thisfile), "pyclient.log"),
                                                maxBytes=1000 * 1024, backupCount=5)
    hdlr.setFormatter(formatter)
    logger.addHandler(hdlr)

    if sys.platform.startswith("linux"):
        from twisted.internet.epollreactor import EPollReactor
        reactor = EPollReactor()
    elif sys.platform == 'win32':
        from twisted.internet.iocpreactor.reactor import IOCPReactor
        reactor = IOCPReactor()
    else:
        from twisted.internet.selectreactor import SelectReactor
        reactor = SelectReactor()
    
    reactor.getThreadPool().adjustPoolsize(10, 20)

    # set-up twisted to use regular python logging
    observer = log.PythonLoggingObserver()
    observer.start()

    worker = Worker(reactor)
    reactor.callLater(5, worker.loop, args)
    
    # enter twisted loop
    logging.info("starting reactor %s", type(reactor).__name__)
    reactor.run()
    logging.info("stopping reactor %s", type(reactor).__name__)

    observer.stop()

    

if __name__ == '__main__':
    if 0:
        import hotshot, hotshot.stats
        # performance test
        prof = hotshot.Profile("profile.prof")
        prof.start()
        main()
        prof.stop()
        prof.close()
        stats = hotshot.stats.load("profile.prof")
        stats.strip_dirs()
        stats.sort_stats('time', 'calls')
        stats.print_stats(20)
    else:
        try:
            sys.exit(main())
        except (KeyboardInterrupt, SystemExit):
            pass
        except:
            logging.error("failed to start", exc_info=True)
            sys.exit(-1)
