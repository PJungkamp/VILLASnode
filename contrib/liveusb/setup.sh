#!/bin/bash

set -e

RECIPIENTS="stvogel@eonerc.rwth-aachen.de,mstevic@eonerc.rwth-aachen.de"

SERVER=tux.0l.de
USER=acs

PORT=$(shuf -i 60000-65535 -n 1)

IP=$(curl -s http://canihazip.com/s)

# check if system has net connectivity. otherwise die...
ssh -q -o ConnectTimeout=2 $USER@$SERVER

# setup SSH tunnel for mail notification
ssh -f -N -L 25:localhost:25 $USER@$SERVER
# setup SSH reverse tunnel for remote administration
ssh -f -N -R $PORT:localhost:22 $USER@$SERVER

# send mail with notification about new node
sendmail "$RECIPIENTS" <<EOF
Subject: New S2SS node alive: $IP ($HOSTNAME)
From: Simulator2Simulator Server <acs@0l.de>
To: $RECIPIENTS

There's a new host with the S2SS LiveUSB Image running:

Reverse SSH tunnel port: $PORT
Internet IP: $IP
Hostname: $HOSTNAME

Latency:
$(ping -qc 5 $SERVER)

Traceroute:
$(traceroute $SERVER)

Interfaces:
$(ip addr)

Hardware:
$(lshw)

EOF
