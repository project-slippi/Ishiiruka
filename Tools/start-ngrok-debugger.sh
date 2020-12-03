#!/bin/bash
#
# Can be used to debug CI issues over SSH if jobs repeatedly fail, and
# can't be reproduced on a local machine.

if [[ -z "$NGROK_TOKEN" ]]; then
  echo "Please set 'NGROK_TOKEN'"
  exit 2
fi

if [[ -z "$NGROK_PASS" ]]; then
  echo "Please set 'NGROK_PASS' for user: $USER"
  exit 3
fi

echo "### Downloading and installing ngrok ###"

# Could fork here to do this on Linux as well, if it's helpful
wget -q https://bin.equinox.io/c/4VmDzA7iaHb/ngrok-stable-darwin-amd64.zip
unzip ngrok-stable-darwin-amd64.zip
chmod +x ./ngrok

echo "### Update user: $USER password ###"
echo -e "$NGROK_PASS\n$NGROK_PASS" | sudo passwd "$USER"

echo "### Start ngrok proxy for 22 port ###"


rm -f .ngrok.log
./ngrok authtoken "$NGROK_TOKEN"
./ngrok tcp 22 --log ".ngrok.log" &

sleep 10
HAS_ERRORS=$(grep "command failed" < .ngrok.log)

if [[ -z "$HAS_ERRORS" ]]; then
  echo ""
  echo "=========================================="
  echo "To connect: $(grep -o -E "tcp://(.+)" < .ngrok.log | sed "s/tcp:\/\//ssh $USER@/" | sed "s/:/ -p /")"
  echo "=========================================="
else
  echo "$HAS_ERRORS"
  exit 4
fi

# Give an hour for debugging~
sleep 1h
