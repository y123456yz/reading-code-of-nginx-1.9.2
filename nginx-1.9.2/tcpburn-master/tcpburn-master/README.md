## A tcp stream replay tool devoted to concurrentcy


#Description
TCPBurn is a replay tool which focuses on concurrency. All TCP-based applications which could be replayed could be stressed by this powerful tool .


#Characteristics
    1) Network latency could be reserved
    2) No need to bind multiple IP addresses and the number of client IP addresses 
       is unlimited
    3) The maximum number of concurrent users is restricted to bandwidth, memory
       and CPU processing power
    4) Only TCP-based applications that could be replayed are supported


#Scenarios
    1) Stress testing 
    2) Comet
    3) Performance testing


#Architecture

![tcpburn](https://raw.github.com/wangbin579/auxiliary/master/images/tcpburn.GIF)

As shown in the above Figure, TCPBurn consists of two parts: *tcpburn* and *intercept*. While *tcpburn* runs on the test server and sends the packets from pcap files, *intercept* runs on the assistant server and does some assistant work, such as passing response info to *tcpburn*.

*tcpburn* reads packets from pcap files and does the necessary processing (including TCP interaction simulation, network latency control, and common upper-layer interaction simulation), and uses raw socket output technique by default to send packets to the target server(pink arrows).

The only operation needed in the target server for TCPBurn is setting appropriate route commands to route response packets(green arrows) to the assistant server.

*intercept* is responsible for passing the response header to *tcpburn*. By capturing the response packets, intercept will extract response header information and send the response header to *tcpburn* using a special channel(purple arrows). When *tcpburn* receives the response header, it utilizes the header information to modify the attributes of pcap packets and continues to send another packet. It should be noticed that the responses from the target server are routed to the assistant server which should act as a black hole.


#tcpburn configure Options
    --with-debug      compile tcpburn with debug support (saved in a log file)
    --pcap-send       send packets at the data link layer instead of the IP layer
    --single          if intercept is configured with "--single" option, so does tcpburn
    --comet           replay for comet applications


#Installation and Running

###1) intercept
    a) Install intercept on the assistant server
      git clone git://github.com/session-replay-tools/intercept.git
      cd intercept
      ./configure --single  
      make
      make install
	
    b) Running intercept on the assistant server(root privilege or the CAP_NET_RAW capability is required):

      ./intercept -F <filter> -i <device,> 
	
      Note that the filter format is the same as the pcap filter.

      For example:

        ./intercept -i eth0 -F 'tcp and src port 80' -d

        Intercept will capture response packets of the TCP-based application which 
      listens on port 80 from device eth0 


###2) Set route commands on the target server which runs server applications

      Set route commands appropriately to route response packets to the assistant server
	
      For example:
	
      Assume 65.135.233.161 is the IP address of the assistant server. We set the following
      route commands to route all responses to the 62.135.200.x's clients to the assistant
      server.

      route add -net 62.135.200.0 netmask 255.255.255.0 gw 65.135.233.161


###3) tcpburn (root privilege or the CAP_NET_RAW capability is required when running)
    a) Install tcpburn on the test server
    git clone git://github.com/session-replay-tools/tcpburn.git
    cd tcpburn

    if not comet scenarios
      ./configure --single 
    else
      ./configure --single --comet  
    endif

    make
    make install
	
    ./tcpburn -x historyServerPort-targetServerIP:targetServerPort -f <pcapfile,> 
     -s <intercept address> -u <user num> -c <ip range,>

    For example:
	
    Assume 65.135.233.160 is the IP address of the target server and 10.110.10.161 is the
    internal IP address of the assistant server and 65.135.233.161 is the external IP 
    address of the assistant server.
      ./tcpburn -x 80-65.135.233.160:80 -f /path/to/80.pcap -s 10.110.10.161 
       -u 10000 -c 62.135.200.x
    
      tcpburn extracts packets from 80.pcap file on dst port 80 and replays these to the
    target server 65.135.233.160 which runs the application listening on port 80. Total 
    sessions replayed are 10000 and client IP addresses used are belonging to 62.135.200.x
    series. tcpburn connects to the assistant server(10.110.10.161) for asking response 
    information.


#Note
    1) All sessions are retrieved from pcap files and make sure the sessions in pcap files 
       are complete.
    2) tcpburn uses raw socket to send packets by default, and if you want to avoid 
       ip_conntrack problems or get better performance, configure tcpburn with "--pcap-send"
       and refer to "./tcpburn -h" for how to set appropriate parameters for running.
    3) The test server and the assistant server could share the same machine.
    4) For comet applications, exclude publish sessions if they exist in pcap files.
    5) tcpburn could not replay TCP-based sessions that could not be replayed, 
       such as SSL/TLS sessions
    6) ip_forward should not be set on the assistant server.
    7) Root privilege or the CAP_NET_RAW capability is required
    8) Please execute "./tcpburn -h" or "./intercept -h" for more details.


##Release History
+ 2014.09  v1.0    TCPBurn released


##Bugs and feature requests
Have a bug or a feature request? [Please open a new issue](https://github.com/session-replay-tools/tcpburn/issues). Before opening any issue, please search for existing issues.


## Copyright and license

Copyright 2014 under [the BSD license](LICENSE).
