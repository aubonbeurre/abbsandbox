package com.abb.mtest;

import org.apache.thrift.async.TAsyncClient;
import org.apache.thrift.async.TAsyncClientManager;
import org.apache.thrift.transport.TNonblockingSocket;
import org.apache.thrift.transport.TTransport;
import org.apache.thrift.transport.TSocket;
import org.apache.thrift.protocol.TBinaryProtocol;
import org.apache.thrift.protocol.TProtocol;
import org.apache.thrift.TException;
import org.apache.thrift.transport.TFramedTransport;
import org.apache.thrift.async.AsyncMethodCallback;
import imaging.Imaging.AsyncClient.mandelbrot_call;
import imaging.Imaging.AsyncClient.transform_call;
import imaging.Imaging;
import imaging.InvalidOperation;
import imaging.Transform;
import java.nio.ByteBuffer;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.ThreadPoolExecutor;
import java.io.IOException;

class MTest {
	public static void main(String[] args) {
		// TTransport transport = new TSocket("localhost", 9090);
		// transport = new TFramedTransport(transport);
        long startOneway = System.nanoTime();

        final LinkedBlockingQueue<Runnable> queue = new LinkedBlockingQueue<Runnable>();
        ExecutorService invoker = new ThreadPoolExecutor(10, 30,
          10, TimeUnit.SECONDS, queue);
        
		for(int i = 0; i < 1000; i++) {
			invoker.execute(new Runnable() {
				public void run()
	            {
					try {
						TNonblockingSocket clientSocket = new TNonblockingSocket(
								"localhost", 9090);
						TAsyncClientManager clientManager = new TAsyncClientManager();
						Imaging.AsyncClient client = new Imaging.AsyncClient(
								new TBinaryProtocol.Factory(), clientManager, clientSocket);
						
						perform(client);
					} catch (InterruptedException x) {
						x.printStackTrace();
					} catch (TException x) {
						x.printStackTrace();
					} catch (IOException x) {
						x.printStackTrace();
					}
			        //System.out.println("Task count.." + queue.size());
	            }
			});	
		}
		
		invoker.shutdown();

		try {
			invoker.awaitTermination(1000, TimeUnit.SECONDS);
		} catch (InterruptedException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		
		long onewayElapsedMillis = (System.nanoTime() - startOneway) / 1000000;
          System.out.println("Success - took " +
                             Long.toString(onewayElapsedMillis) +
                             "ms");

		// transport.open();
		// TProtocol protocol = new TBinaryProtocol(transport);
		// Imaging.Client client = new Imaging.Client(protocol);
		// perform(client);
	}

	private static void perform(final Imaging.AsyncClient client)
			throws TException, InterruptedException {
		final CountDownLatch latch = new CountDownLatch(2);
		final AtomicBoolean returned = new AtomicBoolean(false);

		client.mandelbrot(400, 400,
				new AsyncMethodCallback<Imaging.AsyncClient.mandelbrot_call>() {
					@Override
					public void onComplete(mandelbrot_call response) {
						//System.out.println("complete");
						latch.countDown();

						try {
							client.transform(
									Transform.ROTATE90CCW,
									response.getResult(),
									new AsyncMethodCallback<Imaging.AsyncClient.transform_call>() {
										@Override
										public void onComplete(
												transform_call response) {
											//System.out.println("complete2");
											latch.countDown();
										}

										@Override
										public void onError(Exception exception) {
											System.out.println("exception: "
													+ exception.getMessage());
											latch.countDown();
										}
									});
						} catch (TException e) {
							latch.countDown();
							e.printStackTrace();
						} catch (InvalidOperation e) {
							latch.countDown();
							e.printStackTrace();
						}
					}

					@Override
					public void onError(Exception exception) {
						System.out.println("exception: "
								+ exception.getMessage());
						latch.countDown();
					}
				});

		latch.await(100, TimeUnit.SECONDS);

		// try {
		// ByteBuffer buf = client.mandelbrot(100, 100);
		// buf = client.transform(Transform.ROTATE90CCW, buf);

		// System.out.println("Done");
		// } catch (InvalidOperation io) {
		// System.out.println("Invalid operation: " + io.why);
		// }
	}
}
