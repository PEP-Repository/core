package main

import (
	"log"
	"math/rand"
	"net"
	"time"

	"pep.cs.ru.nl/pep-watchdog/shared"
)

func checkHostname() {
	var randBytes [4]byte
	rand.Seed(time.Now().UnixNano())
	_, err := rand.Read(randBytes[:])
	if err != nil {
		log.Fatalf("Error getting random bytes: %v", err)
	}

	tcpStop := make(chan bool)
	tcpPort := make(chan string)
	go func() {
		defer close(tcpPort)
		ln, err := net.Listen("tcp", shared.Conf.SelfCheckBindAddr)
		if err != nil {
			log.Fatalf("Could not bind tcp to %s: %v",
				shared.Conf.BindAddr, err)
		}
		defer ln.Close()
		_, port, err := net.SplitHostPort(ln.Addr().String())
		if err != nil {
			log.Fatalf("Could not get port number for selfcheck: %v", err)
		}
		tcpPort <- port
		tcpLn, _ := ln.(*net.TCPListener)
		for {
			// We want to check after a second whether the listener should stop
			tcpLn.SetDeadline(time.Now().Add(time.Second))
			conn, err := tcpLn.Accept()

			select {
			case <-tcpStop:
				if conn != nil {
					conn.Close()
				}
				return
			default:
			}

			if err != nil {
				netErr, ok := err.(net.Error)

				// If there is a timeout, because of the deadline we set earlier, just continue
				if !(ok && netErr.Timeout() && netErr.Temporary()) {
					log.Printf("tcp accept error: %v", err)
				}
				continue
			}

			_, err = conn.Write(randBytes[:])
			if err != nil {
				log.Printf("Error writing random bytes to tcp socket: %v", err)
			}
			conn.Close()
		}
	}()

	selfCheckAddr := shared.Conf.SelfCheckAddr
	selfCheckHost, selfCheckPort, err := net.SplitHostPort(selfCheckAddr)
	if err != nil {
		log.Fatalf("Failed parsing SelfCheckAddr: %v", err)
	}

	port, ok := <-tcpPort
	if !ok {
		log.Fatalf("Getting a port to check own hostname failed")
	}
	log.Printf("SelfCheck: listening on port %v", port)

	if selfCheckPort == "0" || selfCheckPort == "" {
		selfCheckAddr = selfCheckHost + ":" + port
	}

	success := false
	//We do 10 attempts to connect to the watchdog itself
	for i := 0; i < 10 && !success; i++ {
		time.Sleep(500 * time.Millisecond)
		conn, err := net.Dial("tcp", selfCheckAddr)
		if err != nil {
			log.Printf("Could not connect to self: %v", err)
			continue
		}
		conn.SetDeadline(time.Now().Add(time.Second))
		var readBytes [4]byte
		_, err = conn.Read(readBytes[:])
		if err != nil {
			log.Printf("Could not read from tcp connection to self: %v", err)
			conn.Close()
			continue
		}
		success = readBytes == randBytes
		if !success {
			log.Printf("Read %x from tcp connection to self, but expected %x", readBytes, randBytes)
		}
		conn.Close()
	}

	tcpStop <- true
	if !success {
		log.Fatalf("Connecting to self to check own hostname failed")
	}
}
