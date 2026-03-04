/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2026 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2026 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 **/


#pragma once

#include <memory>
#include <cstdlib>


namespace Orthanc
{
  template<typename T> class LimitedStdAllocator;

  // TODO-MEM: move this to a dedicated file
  class LimitedMemoryAllocator
  {
  public:
    static void Initialize(size_t maximumMemorySize);
  protected:
    static void* Allocate(size_t elemSize, size_t numElem);

    static void Deallocate(void* p, size_t elemSize, size_t numElem);
    
    template<typename T>friend class LimitedStdAllocator;
  };


  template<typename T>
  class LimitedStdAllocator 
  {
  public:
    typedef T value_type;
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
    typedef const T& const_reference;
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;

    // Rebind to allow allocator for other types
    template<typename U>
    struct rebind 
    {
      typedef LimitedStdAllocator<U> other;
    };

    LimitedStdAllocator() 
    {
    }

    template<typename U>
    LimitedStdAllocator(const LimitedStdAllocator<U>&) 
    {
    }

    pointer allocate(size_type num, const void* hint = 0)
    {
      return static_cast<pointer>(LimitedMemoryAllocator::Allocate(sizeof(T), num));
    }

    void deallocate(pointer p, size_type num)
    {
      LimitedMemoryAllocator::Deallocate(p, sizeof(T), num);
    }

    void construct(pointer p, const T& val) 
    {
      new (p) T(val);
    }

    void destroy(pointer p) 
    {
      p->~T();
    }

    pointer address(reference x) const 
    {
      return &x;
    }

    const_pointer address(const_reference x) const 
    {
      return &x;
    }

    size_type max_size() const 
    {
      return size_type(-1) / sizeof(T);
    }
  };


  // // Equality check
  // template<typename T1, typename T2>
  // bool operator==(const LimitedStdAllocator<T1>&, const LimitedStdAllocator<T2>&)
  // {
  //     return true;
  // }

  // template<typename T1, typename T2>
  // bool operator!=(const LimitedStdAllocator<T1>&, const LimitedStdAllocator<T2>&) throw() {
  //     return false;
  // }

  typedef std::basic_string<char, std::char_traits<char>, LimitedStdAllocator<char> > MemoryManagedString;
}
