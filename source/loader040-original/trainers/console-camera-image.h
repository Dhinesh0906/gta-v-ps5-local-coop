#pragma once
/* Camera state is mutable at 0x2000; all constants and extension code must
 * be compared too before accepting a retained startup chain. (AI-assisted) */
static int split_camera_immutable(int fd,unsigned pid,unsigned long long camera){
    return sizeof(split_camera_image)>=16384&&sizeof(split_camera_image)<=32768&&
        split_immutable(fd,pid,camera,split_camera_image,8192)&&
        split_immutable(fd,pid,camera+12288,split_camera_image+12288,sizeof(split_camera_image)-12288);
}
