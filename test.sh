set -xe

#for i in 128 1024 8192 65536 524288
for i in 256 1024 4096 16384 65536 262144 1048576
do
    for j in 4 8 16 32 64
    do
        make mkfs > /dev/null
        ./efs 7 $i $j
        ./efs 6 $i $j
        ./efs 7 $i $j
        ./efs 8 $i $j
        ./efs 9 $i $j
        sleep 1
    done
done
