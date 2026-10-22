import java.io.FileReader;
import java.io.IOException;
import java.io.BufferedReader;
import java.util.Arrays;
import java.util.Scanner;
import java.util.Set;
import java.util.Iterator;

import java.nio.ByteBuffer;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.net.InetSocketAddress;

public class OracleNode {

    // Parses the config file into matrix and returns number of nodes.
    static int load(String file, int[][] m) throws IOException {
        int cnt = 0;
        for (int[] row : m) Arrays.fill(row, -1);
        try (BufferedReader inp = new BufferedReader(new FileReader(file))) {
            String l;
            while ((l = inp.readLine()) != null) {
                if (l.length() == 0 || l.contains("#"))
                    continue;
                try (Scanner s = new Scanner(l)) {
                    int j = 0;
                    while (s.hasNextInt()) {
                        j++;
                        m[cnt][j + cnt] = s.nextInt();
                    }
                }
                cnt++;
            }
        }
        cnt++;
        for (int t = 0; t < cnt; t++) {
            m[t][t] = 0;
        }
        for (int t = 0; t < cnt; t++) {
            for (int t2 = 0; t2 <= t; t2++) {
                m[t][t2] = m[t2][t];
            }
        }
        return cnt;
    }

    static void sendRow(SocketChannel ch, int node, int n, int[][] matrix,
                        byte[][] ip, int[] port, ByteBuffer buffer) throws IOException {
        buffer.clear();
        for (int i = 0; i < n; i++) {
            if (matrix[node][i] >= 0) {
                buffer.put((byte) ('A' + i));
                buffer.put(ip[i]);
                buffer.putShort((short) port[i]);
                buffer.putInt(matrix[node][i]);
            }
        }
        buffer.flip();
        while (buffer.hasRemaining()) {
            ch.write(buffer);
        }
    }

    public static void main(String[] args) throws IOException {
        if (args.length != 1) {
            System.err.println("Usage: java OracleNode <filename>");
            System.exit(1);
        }
        String file = args[0];
        int[][] matrix = new int[26][26];
        int n = load(file, matrix);

        byte[][] ip = new byte[n][4];
        int[] port = new int[n];
        SocketChannel[] chans = new SocketChannel[26];

        // create a selector
        Selector selector = Selector.open();

        // create a socket for server, bind it to a port and make it non-blocking
        ServerSocketChannel serversoc = ServerSocketChannel.open();
        serversoc.bind(new InetSocketAddress("localhost", 5000));
        serversoc.configureBlocking(false);

        // register the selector to the server socket
        serversoc.register(selector, SelectionKey.OP_ACCEPT);
        ByteBuffer buffer = ByteBuffer.allocate(1024);
        int num = 0, num_connects = 0;
        boolean connects_done = false;

        // wait for all n VNs
        while (!connects_done) {
            selector.select();
            Set<SelectionKey> selectionkeys = selector.selectedKeys();
            Iterator<SelectionKey> iter = selectionkeys.iterator();

            while (iter.hasNext()) {
                SelectionKey key = iter.next();
                iter.remove();

                if (key.isAcceptable()) {
                    // got a vn connection, accept it
                    ServerSocketChannel nusoc = (ServerSocketChannel) key.channel();
                    SocketChannel clichan = nusoc.accept();
                    if (num >= 26) { clichan.close(); continue; }
                    clichan.configureBlocking(false);

                    SelectionKey tmp = clichan.register(selector, SelectionKey.OP_READ|SelectionKey.OP_WRITE);
                    tmp.attach(num);
                    chans[num] = clichan;
                    num++;
                } else if (key.isReadable()) {
                    buffer.clear();
                    SocketChannel client = (SocketChannel) key.channel();
                    int numbytes = client.read(buffer);
                    if (numbytes == -1) {
                        key.cancel();
                        client.close();
                        continue;
                    }
                    buffer.flip();
                    int node = (int) key.attachment();
                    if (node < n && numbytes >= 6) {
                        byte[] ip_node = new byte[4];
                        buffer.get(ip_node);
                        int port_node = buffer.getShort()&0xFFFF;
                        ip[node] = ip_node;
                        port[node] = port_node;
                        System.out.println("Received: "+ (ip_node[0] & 0xFF) + "." + (ip_node[1] & 0xFF) + "." + (ip_node[2] & 0xFF) + "." + (ip_node[3] & 0xFF)+ " " + port_node + " from " + node);
                        num_connects++;
                        if (num_connects >= n) {
                            connects_done = true;
                        }
                    }
                }
            }
        }

        // re-read the config in case it changed during registration
        int[][] cur = new int[26][26];
        int cnt = load(file, cur);
        if (cnt == n) {
            matrix = cur;
        }

        for (int node = 0; node < n; node++) {
            sendRow(chans[node], node, n, matrix, ip, port, buffer);
        }

        // check the file, resend to everyone if different
        while (true) {
            try {
                Thread.sleep(1000);
            } catch (InterruptedException e) {
                break;
            }

            int[][] fresh = new int[26][26];
            int newN;
            try {
                newN = load(file, fresh);
            } catch (Exception e) {
                continue;   // file mid-write or malformed, try again next tick
            }
            if (newN != n) continue;   // node count changed, ignore

            if (!Arrays.deepEquals(fresh, matrix)) {
                matrix = fresh;
                for (int node = 0; node < n; node++) {
                    try {
                        sendRow(chans[node], node, n, matrix, ip, port, buffer);
                    } catch (IOException e) {
                        System.err.println("Send to " + node + " failed");
                    }
                }
                System.out.println("Config changed, update sent");
            }
        }
    }
}