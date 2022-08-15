# Gossip-Style-Membership-Protocol

This repository contains one of the computer programming assignments of Coursera's Cloud Computing Course which is an implementation of a membership protocol based on gossip protocol.
</br>
This membership protocol implementation will sit above an emulated network layer (EmulNet) in a peer- to-peer (P2P) layer, but below an App layer. Think of this like a three-layer protocol stack with Application, P2P, and EmulNet as the three layers. The Application and EmulNet layers where already provide for this assignment.
</br>
This implementation can esily be used in a real world network by changing the two EmulNet and App layers.
</br>
More Information on the details of the design is availible in `mp1_specifications.pdf`.

## How to run
To run this projcet you have to run the following code. You can change the conf file.
```
make
./Aplication ./testcases/singlefailure.conf
```

You can also run all testcases using `run.sh`.
```
chmod +x run.sh
./run.sh
```

You can also test the implementation using `Grader.sh` .
```
chmod +x Grader.sh
./Grader.sh
```
