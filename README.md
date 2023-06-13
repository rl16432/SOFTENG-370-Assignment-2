# SOFTENG370 Assignment 2 2023

## Introduction
In this assignment you have to design your own file system. You are supplied with an interface to a device that provides very little functionality. You can read and write blocks using a block number. So the device looks like an array of blocks. You have to write information about files and directories to the device as well as the file data itself.

## Objective
- To design and implement a file system on top of a simple block device.

## The Device
The virtual disk device that you use to maintain your file system is really just a single file that you have access to via the device.h file. The device provides the bare basics on which you implement your file system. The methods you can call are:
```C
int blockRead(int blockNumber, unsigned char *data);
```
```C
int blockWrite(int blockNumber, unsigned char *data);
```
```C
int numBlocks();
```

There are also some other definitions in that file; a constant, data types and some convenience functions. In particular the `displayBlock` function is useful for seeing what you have actually stored in a block. Read the comments in the `device.h` file for further information. Block numbers go from 0 to the number of blocks minus 1. In this assignment the block size will always be 64 bytes; you can rely on this. So you should pass unsigned char arrays of length 64 as parameters to the block read and write functions.
