#!/usr/bin/env python
import sys
import os
import logging
import getopt
import logging.handlers
import timeit
from optparse import OptionParser, OptionGroup

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
    def loop(self, options):
        try:
            yield self.connect(options)
            
            yield self.doStuff(options)
        except (KeyboardInterrupt, SystemExit):
            pass
        except imaging_constants.InvalidOperation, e:
            logging.error("imaging error: %s (%d)", e.why, e.what, exc_info=True)
        except Exception:
            logging.error("error running loop", exc_info=True)
        
        self.reactor.callLater(0, self.reactor.stop)


    @defer.inlineCallbacks
    def connect(self, options):
        pfactory = TBinaryProtocol.TBinaryProtocolFactory()
        worker_client = ClientCreator(self.reactor, TTwisted.ThriftClientProtocol,
                                    client_class=Imaging.Client, iprot_factory=pfactory)

        while 1:
            try:        
                self.worker_connect = yield worker_client.connectTCP(options.server, options.port)
                break
            except ConnectionRefusedError:        
                logging.warning("could not connect, will retry in 1s")
    
                yield sleep_deferred(self.reactor, 1)
            

    @defer.inlineCallbacks
    def doStuff(self, options):
        if options.test == "simple":
            dims = options.dims
            jpeg = yield self.worker_connect.client.mandelbrot(dims, dims)
            #file(r"J:\mandelbrot.jpg", "wb").write(jpeg)
            
            jpegccw = yield self.worker_connect.client.transform(imaging_constants.Transform.ROTATE90CCW, jpeg)
            #file(r"J:\mandelbrot_ccw.jpg", "wb").write(jpegccw)
            defer.returnValue(None)
            
        elif options.test == "stress":
            yield self.doStress(options)
            defer.returnValue(None)
        
        raise Exception("Unknown command '%s'" % options.test)
        

    @defer.inlineCallbacks
    def __doStressOne(self, res, cnt, dims):
        if isinstance(res, failure.Failure):
            defer.returnValue(res)
        
        worker_connect = res
        logging.debug("start stress #%d", cnt)
        jpeg = yield worker_connect.client.mandelbrot(dims, dims)
        jpegccw = yield worker_connect.client.transform(imaging_constants.Transform.ROTATE90CCW, jpeg)
        logging.debug("end stress #%d", cnt)
        
        worker_connect.transport.loseConnection()

        defer.returnValue(len(jpegccw))


    @defer.inlineCallbacks
    def doStress(self, options):
        start = timeit.default_timer()
        numiter = options.num
        dims = options.dims
        logging.info("starting stress test")
        ds = []    
        pfactory = TBinaryProtocol.TBinaryProtocolFactory()
        for cnt in range(numiter):
            worker_client = ClientCreator(self.reactor, TTwisted.ThriftClientProtocol,
                                        client_class=Imaging.Client, iprot_factory=pfactory)
    
            d = worker_client.connectTCP(options.server, options.port)
            ds.append(d)
            
            d.addBoth(self.__doStressOne, cnt, dims)

        totalsize = yield defer.DeferredList(ds, fireOnOneErrback=True)
        totalsize = map(lambda x: x[1], totalsize)
        totalsize = reduce(lambda x,y: x+y, totalsize)
        totalsize *= 3
        elapsed = timeit.default_timer() - start
        logging.info("stress dims=%dx%d test=stress num=%d elapsed=%.2fs trans=%.2f/s band=%s/s", dims, dims, numiter,
                     elapsed, numiter / elapsed, formatFileSize(totalsize / elapsed))
        

def parse_options():
    try:
        parser = OptionParser(usage="%prog [options]", version="%prog 1.0")
        group = OptionGroup(parser, "Connection Options", "Server address, port...")
        group.add_option("-s", "--server", dest="server", 
                                      help="specify SERVER",
                                      metavar="SERVER",
                                      default="localhost")
        group.add_option("-p", "--port", dest="port", 
                                      help="specify port",
                                      type="int",
                                      default=9090,
                                      metavar="PORT" )
        parser.add_option_group(group)
        
        group = OptionGroup(parser, "Test Options", "Parameters used for the test...")
        group.add_option("-t", "--test", dest="test", 
                                      help="test to run (simple, stress)",
                                      metavar="TEST",
                                      default="simple",
                                      choices=["simple", "stress"])
        group.add_option("-d", "--dims", dest="dims", 
                                      help="image dimensions (400)",
                                      type=int,
                                      metavar="SIZE",
                                      default=400)
        group.add_option("-n", "--num", dest="num", 
                                      help="number of connections for stress (10)",
                                      type=int,
                                      metavar="NUM",
                                      default=10)
        parser.add_option_group(group)
        
        group = OptionGroup(parser, "Debugging Options", "Verbosity, logging...")
        group.add_option("-v", "--verbose", dest="verbose", 
                                      help="verbosity",
                                      action="count",
                                      default=0,
                                      metavar="VERBOSE")
        parser.add_option_group(group)
        
        (options, args) = parser.parse_args()
    except Exception, e:
        logging.error("Error", exc_info=True)
        sys.exit(e)
    
    if args:
        parser.error("too many arguments")

    return options
        
 
def main():
    options = parse_options()
    
    loggingLevel = logging.WARNING
    if options.verbose >= 2:
        loggingLevel = logging.DEBUG
    elif options.verbose >= 1:
        loggingLevel = logging.INFO

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
    reactor.callLater(1, worker.loop, options)
    
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
