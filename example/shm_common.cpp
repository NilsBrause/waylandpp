/*
 * Copyright (c) 2025, Nils Christopher Brause
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "shm_common.hpp"

#include <iostream>
#include <random>
#include <sstream>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

shared_mem_t::shared_mem_t(size_t size)
  : len(size)
{
  // create random filename
  std::stringstream ss;
  std::random_device device;
  std::default_random_engine engine(device());
  std::uniform_int_distribution<unsigned int> distribution(0, std::numeric_limits<unsigned int>::max());
  ss << distribution(engine);
  name = ss.str();

  // open shared memory file
  fd = memfd_create(name.c_str(), 0);
  if(fd < 0)
    throw std::runtime_error("shm_open failed.");

  // set size
  if(ftruncate(fd, size) < 0)
    throw std::runtime_error("ftruncate failed.");

  // map memory
  mem = mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(mem == MAP_FAILED) // NOLINT
    throw std::runtime_error("mmap failed.");
}

shared_mem_t::~shared_mem_t() noexcept
{
  if(fd)
  {
    if(munmap(mem, len) < 0)
      std::cerr << "munmap failed.";
    if(close(fd) < 0)
      std::cerr << "close failed.";
  }
}

int shared_mem_t::get_fd() const
{
  return fd;
}

void *shared_mem_t::get_mem()
{
  return mem;
}
