import sys
import os
import logging
import getopt
import logging.handlers

thisfile = os.path.abspath(os.path.normpath(__file__))
pythonlib = os.path.join(os.path.dirname(os.path.dirname(thisfile)), "pythonlib")
genpy = os.path.join(os.path.dirname(thisfile), "gen-py.twisted")
sys.path.insert(0, pythonlib)
sys.path.insert(0, genpy)

from twisted.internet import defer
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


class Worker(object):
    
    def __init__(self, reactor):
        self.reactor = reactor


    @defer.inlineCallbacks
    def loop(self, cmd):
        try:
            yield self.connect()
            
            yield self.dostuff()
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
    def dostuff(self):
        
        jpeg = yield self.worker_connect.client.mandelbrot(200, 200)
        file(r"J:\mandelbrot.jpg", "wb").write(jpeg)
        
        jpegccw = yield self.worker_connect.client.transform(imaging_constants.Transform.ROTATE90CCW, jpeg)
        file(r"J:\mandelbrot_ccw.jpg", "wb").write(jpegccw)
        

def main():
    def usage():
        theusage = (
            "Usage: python pyclient.py <options>",
            "\nOptional:",
            "  WORKER_QUEUE(S) : list of queues to handle (default: all queues)",
            "  -h : display this help page",
            "  -v : increase logging",
            "",
            )

        sys.stderr.write(string.join(theusage, '\n'))
        sys.stderr.write('\n')

    try:
        if len(sys.argv) <= 0:
            usage()
            return -1

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
        return -1

    # set up the logging
    logger = logging.getLogger()
    formatter = logging.Formatter('%(asctime)s [%(module)s, %(funcName)s, L%(lineno)d] %(levelname)s, %(message)s')

    logger.setLevel(loggingLevel)
    
    hdlr = logging.StreamHandler(sys.stderr)
    hdlr.setFormatter(formatter)
    logger.addHandler(hdlr)
    
    hdlr = logging.handlers.RotatingFileHandler(os.path.join(os.path.dirname(thisfile), "pyclient.log"),
                                                maxBytes=1000*1024, backupCount=5)
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

    # set-up twisted to use regular python logging
    observer = log.PythonLoggingObserver()
    observer.start()

    worker = Worker(reactor)
    reactor.callLater(5, worker.loop, "")
    
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
