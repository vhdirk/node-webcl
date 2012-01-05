/*
** This file contains proprietary software owned by Motorola Mobility, Inc. **
** No rights, expressed or implied, whatsoever to this software are provided by Motorola Mobility, Inc. hereunder. **
** 
** (c) Copyright 2011 Motorola Mobility, Inc.  All Rights Reserved.  **
*/

var cl=require("../webcl"),
    log=console.log;

//First check if the WebCL extension is installed at all 
if (cl == undefined) {
  alert("Unfortunately your system does not support WebCL. " +
  "Make sure that you have the WebCL extension installed.");
  return;
}

VectorAdd();

function VectorAdd() {
  BUFFER_SIZE=10;
  var A=new Uint32Array(BUFFER_SIZE);
  var B=new Uint32Array(BUFFER_SIZE);
  var C=new Uint32Array(BUFFER_SIZE);

  for (var i = 0; i < BUFFER_SIZE; i++) {
    A[i] = i;
    B[i] = i * 2;
    C[i] = 0;
  }

  //Pick platform
  var platformList=cl.getPlatforms();
  platform=platformList[0];

  //Query the set of devices on this platform
  devices = platform.getDevices(cl.DEVICE_TYPE_ALL);

  // create GPU context for this platform
  context=cl.createContext(cl.DEVICE_TYPE_GPU, [cl.CONTEXT_PLATFORM, platform]);

  kernelSourceCode = [
"__kernel void vadd(__global int *a, __global int *b, __global int *c, int iNumElements) ",
"{                                                                           ",
"    size_t i =  get_global_id(0);                                           ",
"    if(i > iNumElements) return;                                            ",
"    c[i] = a[i] + b[i];                                                     ",
"}                                                                           "
].join("\n");

  //Create and program from source
  program=context.createProgram(kernelSourceCode);

  //Build program
  program.build(devices,"");

  size=BUFFER_SIZE*Uint32Array.BYTES_PER_ELEMENT; // size in bytes
  
  //Create kernel object
  try {
    kernel= program.createKernel("vadd");
  }
  catch(err) {
    console.log(program.getBuildInfo(devices[0],cl.PROGRAM_BUILD_LOG));
  }
  
  //Create command queue
  queue=context.createCommandQueue(devices[0], 0);

  //Create buffer for A and copy host contents
  //aBuffer = context.createBuffer(cl.MEM_READ_ONLY, size);
  aBuffer = context.createBuffer(cl.MEM_READ_WRITE, size);
  map=queue.enqueueMapBuffer(aBuffer, cl.TRUE, cl.MAP_WRITE, 0, BUFFER_SIZE * Uint32Array.BYTES_PER_ELEMENT);
  var buf=new Uint32Array(map.getBuffer());
  for(var i=0;i<BUFFER_SIZE;i++) {
    buf[i]=A[i];
  }
  queue.enqueueUnmapMemObject(aBuffer, map);

  //Create buffer for B and copy host contents
  //bBuffer = context.createBuffer(cl.MEM_READ_ONLY, size);
  bBuffer = context.createBuffer(cl.MEM_READ_WRITE, size);
  map=queue.enqueueMapBuffer(bBuffer, cl.TRUE, cl.MAP_WRITE, 0, BUFFER_SIZE * Uint32Array.BYTES_PER_ELEMENT);
  buf=new Uint32Array(map.getBuffer());
  for(var i=0;i<BUFFER_SIZE;i++) {
    buf[i]=B[i];
  }
  queue.enqueueUnmapMemObject(bBuffer, map);

  //Create buffer for that uses the host ptr C
  cBuffer = context.createBuffer(cl.MEM_READ_WRITE, size);

  //Set kernel args
  kernel.setArg(0, aBuffer, cl.type.MEM);
  kernel.setArg(1, bBuffer, cl.type.MEM);
  kernel.setArg(2, cBuffer, cl.type.MEM);
  kernel.setArg(3, BUFFER_SIZE, cl.type.INT | cl.type.UNSIGNED);

  // Init ND-range
  var localWS = [5];
  var globalWS = [Math.ceil (BUFFER_SIZE / localWS) * localWS];

  log("Global work item size: " + globalWS);
  log("Local work item size: " + localWS);

  // Execute (enqueue) kernel
  queue.enqueueNDRangeKernel(kernel,
      [],
      [globalWS],
      [localWS]);
  
  //printResults(A,B,C);
  //There is no need to perform a finish on the final unmap
  //or release any objects as this all happens implicitly with
  //the C++ Wrapper API.
  
  log("using enqueueMapBuffer");
  // Map cBuffer to host pointer. This enforces a sync with 
  // the host backing space, remember we choose GPU device.
  map=queue.enqueueMapBuffer(
      cBuffer,
      cl.TRUE, // block 
      cl.MAP_READ,
      0,
      BUFFER_SIZE * Uint32Array.BYTES_PER_ELEMENT);
  
  buf=new Uint32Array(map.getBuffer());
  for(var i=0;i<BUFFER_SIZE;i++) {
    C[i]=buf[i];
  }

  queue.enqueueUnmapMemObject(cBuffer, map);
  
  queue.finish (); //Finish all the operations

  printResults(A,B,C);
}

function printResults(A,B,C) {
  //Print input vectors and result vector
  var output = "\nA = "; 
  for (var i = 0; i < BUFFER_SIZE; i++) {
    output += A[i] + ", ";
  }
  output += "\nB = ";
  for (var i = 0; i < BUFFER_SIZE; i++) {
    output += B[i] + ", ";
  }
  output += "\nC = ";
  for (var i = 0; i < BUFFER_SIZE; i++) {
    output += C[i] + ", ";
  }

  log(output);
}