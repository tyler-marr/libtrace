#!/usr/bin/python
import libtrace
import sys

trace = libtrace.Trace(sys.argv[1])

print "trace=",trace


packet = libtrace.Packet()

while 1:
	trace.trace_read_packet(packet)
	if not packet:
		break
	#ippacket = packet.trace_get_ip()
	#if not ippacket:
#		continue

	print packet.size
	#print ippacket.ip_src,'->',ippacket.ip_dst

