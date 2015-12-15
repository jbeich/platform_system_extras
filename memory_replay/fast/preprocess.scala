#!/bin/sh
  exec scala -J-Xmx8g "$0" "$@"
!#

/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import java.io.{ByteArrayInputStream, ByteArrayOutputStream, PrintStream}
import java.nio.{ByteBuffer, ByteOrder}
import java.nio.channels.FileChannel
import java.nio.file.{FileSystems, Path, StandardOpenOption}
import scala.collection.mutable.{HashMap, HashSet, MutableList}
import scala.io.{BufferedSource, Source}

case class Address(address: String)
case class Thread(id: Int) extends Ordered[Thread] {
  def compare(that: Thread) = this.id - that.id
}
case class Pointer(id: Int)
case class LocalSyncPoint(id: Int)
case class GlobalSyncPoint(id: Int)
case class Allocation(id: Int, var allocatingThread: Thread)

abstract class Command(commandType: Int, arg1: Int, arg2: Int, arg3: Int) {
  def write(buffer: ByteBuffer) {
    buffer.put(commandType.asInstanceOf[Byte])
    buffer.putInt(arg1).putInt(arg2).putInt(arg3)
  }
}

case class Malloc(result: Pointer, size: Int) extends Command(0, result.id, size, 0)
case class Calloc(result: Pointer, count: Int, size: Int) extends Command(1, result.id, count, size)
case class Memalign(result: Pointer, alignment: Int, size: Int) extends Command(2, result.id, alignment, size)
case class Realloc(result: Pointer, size: Int) extends Command(3, result.id, size, 0)
case class Free(pointer: Pointer) extends Command(4, pointer.id, 0, 0)
case class Sync(syncPoint: LocalSyncPoint) extends Command(5, syncPoint.id, 0, 0)
case class Dump(syncPoint: GlobalSyncPoint, lineNumber: Int) extends Command(6, syncPoint.id, lineNumber, 0)
case class ThreadStart(syncPoint: GlobalSyncPoint, thread: Thread) extends Command(7, syncPoint.id, thread.id, 0)
case class ThreadExit(syncPoint: GlobalSyncPoint, thread: Thread) extends Command(8, syncPoint.id, thread.id, 0)

case class MemoryReplay(activeThreads: HashSet[Thread], cmds: HashMap[Thread, MutableList[Command]],
                        allocationCount: Int, localSyncCount: Int,
                        globalSyncPoints: MutableList[Int])

def error(msg: String) {
  Console.err.println(msg)
  System.exit(1)
}

def warn(msg: String) {
  Console.err.println(msg)
}

def orderedTuple[T <: Ordered[T]](t: (T, T)): (T, T) = {
  if (t._1 < t._2) {
    t
  } else {
    (t._2, t._1)
  }
}

def parseReplay(input: Iterator[String], dumpInterval: Option[Int] = Some(100000)): MemoryReplay = {
  var threads = HashMap[String, Thread]()
  var activeThreads = HashSet[Thread]()
  var commands = HashMap[Thread, MutableList[Command]]()
  var allocations = HashMap[Address, Allocation]()
  var allocationIndex = 0

  def newAllocation(address: String)(implicit thread: Thread) = {
    val addr = Address(address)
    if (allocations.contains(addr)) {
      error("ERROR: Preexisting address returned by allocation")
    }

    val allocation = Allocation(allocationIndex, thread)
    allocations(addr) = allocation
    allocationIndex += 1

    Pointer(allocation.id)
  }

  def appendCommand(command: Command)(implicit thread: Thread) {
    if (!commands.contains(thread)) {
      commands(thread) = MutableList[Command]()
    }
    commands(thread) += command
  }

  var localSyncPoints = HashMap[(Thread, Thread), LocalSyncPoint]()
  def localSync(originalThread: Thread)(implicit currentThread: Thread) {
    if (originalThread != currentThread) {
      val key = orderedTuple((originalThread, currentThread))
      if (!localSyncPoints.contains(key)) {
        localSyncPoints(key) = LocalSyncPoint(localSyncPoints.size)
      }

      val localSyncPoint = localSyncPoints(key)
      appendCommand(Sync(localSyncPoint))(originalThread)
      appendCommand(Sync(localSyncPoint))(currentThread)
    }
  }

  var globalSyncPointCounts = MutableList[Int]()
  def globalSync() = {
    val globalSyncPoint = GlobalSyncPoint(globalSyncPointCounts.length)
    globalSyncPointCounts += activeThreads.size
    globalSyncPoint
  }

  var threadCount = 0
  def getThread(originalThread: String): Thread = {
    if (threads.contains(originalThread)) {
      threads(originalThread)
    } else {
      // If the thread doesn't exist, create a global sync point and tell all of the active threads.
      val thread = Thread(threadCount)
      threadCount += 1

      if (thread.id != 0) {
        // Don't do this for the first thread.
        val syncPoint = globalSync()
        activeThreads.foreach {
          appendCommand(ThreadStart(syncPoint, thread))(_)
        }
      }

      threads(originalThread) = thread
      activeThreads += thread
      thread
    }
  }

  def dump(lineNumber: Int) {
    val syncPoint = globalSync()
    threads.values foreach { thread =>
      appendCommand(Dump(syncPoint, lineNumber))(thread)
    }
  }

  var lineNumber = 0
  for (line <- input) {
    lineNumber += 1
    if (lineNumber % 10000 == 0) {
      println("Parsing line %d".format(lineNumber))
    }

    val linePattern = "(\\d+): (.+)".r
    val malloc = "malloc ([\\S]+) (\\d+)".r
    val calloc = "calloc ([\\S]+) (\\d+) (\\d+)".r
    val memalign = "memalign ([\\S]+) (\\d+) (\\d+)".r
    val realloc = "realloc ([\\S]+) ([\\S]+) (\\d+)".r
    val free = "free ([\\S]+)".r
    val threadExit = "thread_done [\\S]+".r

    line match {
      case linePattern(threadId, command) =>
        implicit val thread = getThread(threadId)
        import Integer.parseInt
        command match {
          case malloc(pointer, size) =>
            appendCommand(Malloc(newAllocation(pointer), Integer.parseInt(size)))

          case calloc(pointer, count, size) =>
            appendCommand(Calloc(newAllocation(pointer), Integer.parseInt(count), Integer.parseInt(size)))

          case memalign(pointer, alignment, size) =>
            appendCommand(Memalign(newAllocation(pointer), Integer.parseInt(alignment), Integer.parseInt(size)))

          case realloc(result, original, size) =>
            if (original == "0" || original == "0x0") {
              appendCommand(Malloc(newAllocation(result), Integer.parseInt(size)))
            } else {
              val oldPointer = Address(original)
              val newPointer = Address(result)
              if (!allocations.contains(oldPointer)) {
                error("ERROR[%d]: realloc on a nonexisting pointer".format(lineNumber))
              }
              val allocation = allocations(oldPointer)

              localSync(allocation.allocatingThread)

              allocation.allocatingThread = thread
              allocations.remove(oldPointer)
              allocations(newPointer) = allocation

              appendCommand(Realloc(Pointer(allocation.id), Integer.parseInt(size)))
            }

          case free(pointer) =>
            if (pointer != "0" && pointer != "0x0") {
              val address = Address(pointer)
              if (!allocations.contains(address)) {
                error("ERROR[%d]: freed non-allocated pointer".format(lineNumber))
              }
              val allocation = allocations(address)
              val otherThread = allocation.allocatingThread
              if (activeThreads.contains(otherThread)) {
                localSync(otherThread)
              }
              allocations.remove(address)
              appendCommand(Free(Pointer(allocation.id)))
            }

          case threadExit() =>
            val syncPoint = globalSync()
            activeThreads.foreach {
              appendCommand(ThreadExit(syncPoint, thread))(_)
            }
            activeThreads.remove(thread)
            threads.remove(threadId)

          case unhandled =>
            warn("WARNING[%d]: unhandled command %s".format(lineNumber, unhandled))
        }
    }

    dumpInterval foreach { x: Int =>
      if (lineNumber % x == 0) {
        dump(lineNumber)
      }
    }
  }

  MemoryReplay(activeThreads, commands, allocationIndex, localSyncPoints.size, 
               globalSyncPointCounts)
}

class BinaryWriter(replay: MemoryReplay) {
  def getHeader() = {
    // struct file_header {
    //   char magic[8] = "MEM_RPLY";
    //   uint32_t allocation_count;
    //   uint32_t thread_count;
    //   uint32_t final_thread_count;
    //   uint32_t local_sync_count;
    //   uint32_t global_sync_count;
    //   uint32_t global_sync_counts[global_sync_count];
    // };

    val buffer = ByteBuffer.allocate(28 + 4 * replay.globalSyncPoints.length)
                           .order(ByteOrder.LITTLE_ENDIAN)
    println("Allocation count: %d".format(replay.allocationCount))
    println("Total thread count: %d".format(replay.cmds.size))
    println("Final thread count: %d".format(replay.activeThreads.size))
    println("Local sync count: %d".format(replay.localSyncCount))
    println("Global sync count: %d".format(replay.globalSyncPoints.length))
    buffer.put("MEM_RPLY".getBytes("UTF8"), 0, 8)
    buffer.putInt(replay.allocationCount)
    buffer.putInt(replay.cmds.size)
    buffer.putInt(replay.activeThreads.size)
    buffer.putInt(replay.localSyncCount)
    buffer.putInt(replay.globalSyncPoints.length)
    replay.globalSyncPoints.foreach(buffer.putInt)
    buffer.position(0)
    buffer
  }

  def writeThread(thread: Thread): ByteBuffer = {
    // struct thread_header {
    //   uint16_t thread_id;
    //   uint32_t command_count;
    //   command commands[0];
    // }
    val commands = replay.cmds(thread)
    val bufferLength = 2 + 4 + 13 * commands.length
    val buffer = ByteBuffer.allocate(bufferLength).order(ByteOrder.LITTLE_ENDIAN)
    if (thread.id > 32767) {
      throw new IllegalArgumentException("Thread ID overflowed")
    }

    buffer.putShort(thread.id.asInstanceOf[Short])
    buffer.putInt(commands.length)
    commands.foreach { _.write(buffer) }
    assert(buffer.position == buffer.capacity)
    buffer.position(0)
    buffer
  }

  def write(path: Path) = {
    val header = getHeader()
    val threads = replay.cmds.keys.toSeq.sortBy(_.id)
    val threadBuffers = threads.map(writeThread)
    val totalSize = header.capacity + threadBuffers.map(_.capacity).reduce(_+_)

    println("Total size = %d".format(totalSize))
    val fileChannel = FileChannel.open(path, StandardOpenOption.CREATE,
                                       StandardOpenOption.TRUNCATE_EXISTING,
                                       StandardOpenOption.READ, StandardOpenOption.WRITE)
    fileChannel.truncate(totalSize)

    val result = fileChannel.map(FileChannel.MapMode.READ_WRITE, 0, totalSize)
    result.put(header)
    threadBuffers.foreach(result.put)
    result
  }
}

if (args.length != 2) {
  error("Usage: preprocess.scala <filename> <output filename>")
}

val file = Source.fromFile(args(0))
val parsed = parseReplay(file.getLines())
val outputPath = FileSystems.getDefault().getPath(args(1))
new BinaryWriter(parsed).write(outputPath)
