echo starting fs
./fs &
PID=$!
sleep 1

echo cd mnt/
cd mnt
sleep 1

echo ls
ls
sleep 1

echo cd ..
cd ..
sleep 1

echo c-C
kill $PID
