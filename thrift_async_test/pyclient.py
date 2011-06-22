from twisted.internet import defer
from twisted.internet.protocol import ClientCreator
from twisted.internet.selectreactor import SelectReactor
from twisted.python import log, failure

from thrift.transport import TTwisted

pfactory = TBinaryProtocol.TBinaryProtocolFactory()
worker_client = ClientCreator(self.reactor, TTwisted.ThriftClientProtocol,
                            client_class=WorkerService.Client, iprot_factory=pfactory)
