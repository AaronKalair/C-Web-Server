A Simple Webserver Written in C
===============================

This is a simple webserver written in the C programming language.

It uses a pool of 10 connections to serve multiple requests concurrently and it keeps track of how much data it has output, printing it to the standard output stream.


Compiling and Using the System
==============================

On a Linux system system simply use the makefile to compile the server.

On a Mac use this command to compile the server:

`gcc server.c -o server`


To run the server type `./server` into a terminal that is in the directory where the executable file is located.

You can also pass a number after ./server to indicate how many threads the server should use like: `./server 3` for 3 child-processes

By default the server runs on port 2001, so to try it out navigate to

localhost:2001 in a webbrowser
