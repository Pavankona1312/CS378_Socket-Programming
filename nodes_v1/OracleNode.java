import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.BufferedReader;
import java.io.PrintWriter;
import java.util.Scanner;
import java.util.Set;        // Try to rewrite using other list types
import java.util.Iterator;

import java.nio.ByteBuffer;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.net.InetSocketAddress;



public class OracleNode {
 public static void main(String[] args) throws IOException{
    if(args.length != 1){
            System.err.println(
                "Usage: java OracleNode <filename>");
            System.exit(1);
    }
    String file = args[0];
    int matrix[][] = new int[26][26];
    int n=0;

    try (
        BufferedReader inp = new BufferedReader(new FileReader(file));
        PrintWriter out = new PrintWriter(new FileWriter("debug.txt"));
    ){
        String l;
        while((l=inp.readLine()) != null){
            if(l.length()==0||l.contains("#"))
                continue;
            out.println(l);
            Scanner s = new Scanner(l);
            int j=0;
            while(s.hasNextInt()){
                j++;
                matrix[n][j+n]=s.nextInt();
            }
            n++;
        }
        n++;
    }
    for(int t=0;t<n;t++){
        matrix[t][t]=0;
    }
    for(int t = 0; t<n ;t++){
        for(int t2=0;t2<=t;t2++){
            matrix[t][t2]=matrix[t2][t];
        }
    }
    String info[] = new String[n];
    // create a selector
    Selector selector = Selector.open();

    // create a socket for server, bind it to a port and make it non-blocking
    ServerSocketChannel serversoc = ServerSocketChannel.open();
    serversoc.bind(new InetSocketAddress("localhost",5000));
    serversoc.configureBlocking(false);

    // register the selector to the server socket
    serversoc.register(selector, SelectionKey.OP_ACCEPT);
    ByteBuffer buffer = ByteBuffer.allocate(1024);
    int num = 0, num_connects=0;
    while(true){
        selector.select();
        Set<SelectionKey> selectionkeys = selector.selectedKeys();
        Iterator<SelectionKey> iter= selectionkeys.iterator();
        Boolean connects_done=false;

        while(iter.hasNext()){
            SelectionKey key = iter.next();
            if(key.isAcceptable()){
                // got a vn connection !! accept it
                ServerSocketChannel nusoc = (ServerSocketChannel)key.channel();
                SocketChannel clichan = nusoc.accept(); clichan.configureBlocking(false);
                
                // register the channel to the selector
                SelectionKey tmp = clichan.register(selector, SelectionKey.OP_READ | SelectionKey.OP_WRITE);
                tmp.attach(num);
                num++;
            }

            if(key.isReadable()){
                buffer.clear();
                SocketChannel client = (SocketChannel)key.channel();
                int numbytes = client.read(buffer);
                if(numbytes==-1){
                    key.cancel();
                    client.close();
                    continue;
                }
                buffer.flip();
                String recmsg = new String(buffer.array(),0,numbytes);
                int node = (int)key.attachment();
                System.out.println("Recieved: " + recmsg + " from " + node);
                if(node<n){
                    info[node] = recmsg;
                    num_connects++;
                    if(num_connects>=n){
                        connects_done=true;
                    }
                }
                if(connects_done) break;
            }
            iter.remove();
        }
        if(connects_done) break;
    }
    try (
        BufferedReader inp = new BufferedReader(new FileReader(file));
        PrintWriter out = new PrintWriter(new FileWriter("debug.txt"));
    ){
        n=0;
        String l;
        while((l=inp.readLine()) != null){
            if(l.length()==0||l.contains("#"))
                continue;
            out.println(l);
            Scanner s = new Scanner(l);
            int j=0;
            while(s.hasNextInt()){
                j++;
                matrix[n][j+n]=s.nextInt();
            }
            n++;
        }
        n++;
    }
    for(int t=0;t<n;t++){
        matrix[t][t]=0;
    }
    for(int t = 0; t<n ;t++){
        for(int t2=0;t2<=t;t2++){
            matrix[t][t2]=matrix[t2][t];
        }
    }
    buffer.clear();
    Set<SelectionKey> allkeys = selector.keys();
    Iterator<SelectionKey> all_keys_iter = allkeys.iterator();
    while(all_keys_iter.hasNext()){
        SelectionKey key = all_keys_iter.next();
        if(key.isWritable()){
            int node = (int)key.attachment();
            if(node>=n) {all_keys_iter.remove(); continue;}
            buffer.clear();
            SocketChannel clichan = (SocketChannel)key.channel();
            // int[] neighbours = matrix[node];
            for(int i=0;i<n;i++){
                if(matrix[node][i]>=0){
                    char letter = (char) ('A' + i);
                    String message = letter + " " + info[i] + " " + matrix[node][i] + "\n";
                    buffer.put(message.getBytes(java.nio.charset.StandardCharsets.UTF_8));
                }
            }
            buffer.flip();
            clichan.write(buffer);
        }
        // all_keys_iter.remove();
    }
    
 }
}
