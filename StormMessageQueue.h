#pragma once

#include "StormGenIndex.h"

#include <atomic>
#include <memory>
#include <thread>
#include <stdint.h>

#include "StormProfiling.h"

namespace StormSockets
{
	template <typename T>
	struct StormMessageContainer
	{
		T MessageInfo;
		volatile int HasData;
	};

	template <typename T>
	struct StormMessageMegaContainer
	{
		T MessageInfo;
		StormGenIndex HasData;
	};

	template <typename T>
	class StormMessageQueue
	{
	private:
    StormGenIndex m_Head;
		volatile int m_Tail;
    volatile int m_Cycles;
		std::unique_ptr<int[]> m_Queue;
		std::unique_ptr<StormMessageContainer<T>[]> m_Array;
		int m_Length;

	public:
		StormMessageQueue(int size)
		{
      m_Tail = 0;
      m_Head = StormGenIndex(0, 1);
      m_Cycles = 1;

			m_Queue = std::make_unique<int[]>(size);
			m_Array = std::make_unique<StormMessageContainer<T>[]>(size);
			m_Length = size;

			for (int index = 0; index < size; index++)
			{
				m_Array[index].HasData = 0;
        m_Queue[index] = -m_Cycles;
			}
		}

		int AllocateArraySlot()
		{
			int message_index = -1;
			for (int index = 0; index < m_Length; index++)
			{
				int new_val = 1;
				int old_val = 0;
				if (std::atomic_compare_exchange_weak((std::atomic_int *)&m_Array[index].HasData, &old_val, new_val))
				{
					message_index = index;
					break;
				}
			}

			return message_index;
		}

		void ReleaseArraySlot(int message_index)
		{
			m_Array[message_index].HasData = 0;
		}

		void Reset()
		{
			m_Tail = 0;
      m_Head = StormGenIndex(0, 1);
      m_Cycles = 1;

			for (int index = 0; index < m_Length; index++)
			{
				m_Queue[index] = -m_Cycles;
			}
		}

    bool Enqueue(const T & message)
    {
      int message_index = AllocateArraySlot();
      if (message_index == -1)
      {
        return false;
      }

      m_Array[message_index].MessageInfo = message;
      if (InsertMessageIndex(message_index) == false)
      {
        ReleaseArraySlot(message_index);
        return false;
      }

      return true;
    }

    bool Enqueue(T && message)
    {
      int message_index = AllocateArraySlot();
      if (message_index == -1)
      {
        return false;
      }

      m_Array[message_index].MessageInfo = message;
      if (InsertMessageIndex(message_index) == false)
      {
        ReleaseArraySlot(message_index);
        return false;
      }

      return true;
    }

		bool HasData()
		{
			return m_Tail != m_Head.GetIndex();
		}

		bool TryDequeue(T & output)
		{
			int idx = m_Tail;
			if (idx == m_Head.GetIndex())
			{
				return false;
			}

      if (idx == 0)
      {
        int new_cycles = (m_Cycles + 2) & 0xF;
        m_Cycles = new_cycles;
      }

			int val = m_Queue[idx];
			int new_tail = (idx + 1) % m_Length;

      output = m_Array[val].MessageInfo;
      m_Queue[idx] = -m_Cycles;
			m_Array[val].HasData = 0;
      m_Tail = new_tail;
			return true;
		}

  private:

    bool InsertMessageIndex(int message_index)
    {
      while (true)
      {
        auto old_head = StormGenIndex(m_Head);

        int idx = old_head.GetIndex();
        int start_cycles = old_head.GetGen();

        int new_head = (idx + 1) % m_Length;
        if (new_head == m_Tail)
        {
          return false;
        }

        int old_val = -start_cycles;
        if (std::atomic_compare_exchange_weak((std::atomic_int *)&m_Queue[idx], &old_val, message_index))
        {
          while (true)
          {
            new_head = (idx + 1) % m_Length;
            int new_head_cycles = (new_head != 0 ? old_head.GetGen() : old_head.GetGen() + 2) & 0xF;

            StormGenIndex new_head_index = StormGenIndex(new_head, new_head_cycles);

            if (std::atomic_compare_exchange_weak((std::atomic_int *)&m_Head, (int *)&old_head.Raw, (int)new_head_index.Raw))
            {
              return true;
            }
          }
        }
      }
    }

	};

	template <typename T>
	struct StormMessageMegaQueue
	{
		StormDoubleGenIndex m_Head;
		volatile int m_Tail;
		volatile int m_Cycles;
		std::atomic_int m_ArrayStart;
		int m_Size;
		int m_Offset;
		int m_EndIndex;

		StormMessageMegaQueue()
		{

		}

		void Init(StormGenIndex * queue, StormMessageMegaContainer<T> * array, int offset, int size)
		{
			m_Offset = offset;
			m_EndIndex = offset + size;
			m_Size = size;
			m_Tail = 0;
			m_Head = StormDoubleGenIndex(0, 0, 1);
			m_Cycles = 1;
			m_ArrayStart = 0;

			for (int index = m_Offset; index < m_EndIndex; index++)
			{
				queue[index] = StormGenIndex(-m_Cycles, 0);
				array[index].HasData.Raw = 0;
			}
		}

		int AllocateArraySlot(int gen, StormMessageMegaContainer<T> * array)
		{
			uint64_t prof = Profiling::StartProfiler();
			m_ArrayStart.fetch_add(1);

			for (int array_slot = 0; array_slot < m_Size; array_slot++)
			{
				int index = (array_slot + m_ArrayStart) % m_Size + m_Offset;
				StormGenIndex gen_index = array[index].HasData;
				if (gen_index.GetGen() != gen)
				{
					Profiling::EndProfiler(prof, ProfilerCategory::kAllocArraySlot);
					return -1;
				}

				if (gen_index.GetIndex() != 0)
				{
					continue;
				}

				StormGenIndex new_index = StormGenIndex(1, gen);
				if (std::atomic_compare_exchange_weak((std::atomic_int *)&array[index].HasData.Raw, (int *)&gen_index.Raw, (int)new_index.Raw))
				{
					Profiling::EndProfiler(prof, ProfilerCategory::kAllocArraySlot);
					return index;
				}
			}

			Profiling::EndProfiler(prof, ProfilerCategory::kAllocArraySlot);
			return -1;
		}

		void ReleaseArraySlot(int message_index, StormMessageMegaContainer<T> * array)
		{
			uint64_t prof = Profiling::StartProfiler();
			while (true)
			{
				StormGenIndex data_marker = array[message_index].HasData;
				StormGenIndex new_index = StormGenIndex(0, data_marker.GetGen());

				if (std::atomic_compare_exchange_weak((std::atomic_int *)&array[message_index].HasData.Raw, (int *)&data_marker.Raw, (int)new_index.Raw))
				{
					Profiling::EndProfiler(prof, ProfilerCategory::kReleaseArraySlot);
					return;
				}
			}
		}

		bool Enqueue(T message, int gen, StormGenIndex * queue, StormMessageMegaContainer<T> * array)
		{
			uint64_t prof = Profiling::StartProfiler();

			// First allocate a slot for the data to go
			int message_index = AllocateArraySlot(gen, array);
			if (message_index <= -1)
			{
				Profiling::EndProfiler(prof, ProfilerCategory::kEnqueue);
				return false;
			}

			array[message_index].MessageInfo = message;
			StormGenIndex new_queue_index = StormGenIndex(message_index, gen);

			// Next allocate a queue slot that links to the data
			while (true)
			{
        std::atomic_thread_fence(std::memory_order_seq_cst);

				StormDoubleGenIndex old_head = m_Head;
				if (old_head.GetGen1() != gen)
				{
					ReleaseArraySlot(message_index, array);
					Profiling::EndProfiler(prof, ProfilerCategory::kEnqueue);
					return false;
				}

				int start_cycles = old_head.GetGen2();

				int idx = old_head.GetIndex();
				int new_head = (idx + 1) % m_Size;

				if (new_head == m_Tail)
				{
					ReleaseArraySlot(message_index, array);
					Profiling::EndProfiler(prof, ProfilerCategory::kEnqueue);
					return false;
				}

				idx += m_Offset;
				StormGenIndex prev_queue_index = queue[idx];
				if (prev_queue_index.GetGen() != gen)
				{
					ReleaseArraySlot(message_index, array);
					Profiling::EndProfiler(prof, ProfilerCategory::kEnqueue);
					return false;
				}

				if (prev_queue_index.GetIndex() != -start_cycles)
				{
					continue;
				}

				if (std::atomic_compare_exchange_weak((std::atomic_int *)&queue[idx].Raw, (int *)&prev_queue_index.Raw, (int)new_queue_index.Raw))
				{
					// Finally, advance the queue head by one
					while (true)
					{
						int new_head_index = (old_head.GetIndex() + 1) % m_Size;
						int new_gen_2 = (new_head_index != 0 ? old_head.GetGen2() : old_head.GetGen2() + 2) & 0xF;

						StormDoubleGenIndex new_index = StormDoubleGenIndex(new_head_index, gen, new_gen_2);

						if (old_head.GetGen1() != gen)
						{
							ReleaseArraySlot(message_index, array);
							Profiling::EndProfiler(prof, ProfilerCategory::kEnqueue);
							return false;
						}

						if (std::atomic_compare_exchange_weak((std::atomic_int *)&m_Head.Raw, (int *)&old_head.Raw, (int)new_index.Raw))
						{
							Profiling::EndProfiler(prof, ProfilerCategory::kEnqueue);
							return true;
						}

            std::this_thread::yield();
					}
				}
			}
		}

		bool HasData()
		{
			return m_Tail != m_Head.GetIndex();
		}

		bool TryDequeue(T & output, int gen, StormGenIndex * queue, StormMessageMegaContainer<T> * array)
		{
			// Advance the tail by one
			int idx = m_Tail;
			if (idx == m_Head.GetIndex())
			{
				return false;
			}

			if (idx == 0)
			{
				int new_cycles = (m_Cycles + 2) & 0xF;
				m_Cycles = new_cycles;
			}

			int new_tail = (idx + 1) % m_Size;
			idx += m_Offset;

			// Read the slot id for the data at the tail
			int val = queue[idx].GetIndex();
			output = array[val].MessageInfo;

			// Free the queue slot
			StormGenIndex new_queue_val = StormGenIndex(-(m_Cycles), gen);
			queue[idx] = new_queue_val;

			StormGenIndex new_val = StormGenIndex(0, gen);
			array[val].HasData = new_val;

			m_Tail = new_tail;
			return true;
		}

		bool PeekTop(T & output, int gen, StormGenIndex * queue, StormMessageMegaContainer<T> * array, int offset)
		{
			if (offset >= m_Size)
			{
				return false;
			}

			int idx = m_Tail + offset;
			int head = m_Head.GetIndex();

			if (head >= m_Tail)
			{
				if (idx >= head)
				{
					return false;
				}
			}
			else
			{
				if (idx >= m_Size)
				{
					idx -= m_Size;

					if (idx >= head)
					{
						return false;
					}
				}
			}

			idx += m_Offset;

			StormGenIndex queue_val = queue[idx];

			int val = queue_val.GetIndex();
			output = array[val].MessageInfo;
			return true;
		}

    void ReplaceTop(const T & value, int gen, StormGenIndex * queue, StormMessageMegaContainer<T> * array, int offset)
    {
      if (offset >= m_Size)
      {
        return;
      }

      int idx = m_Tail + offset;
      int head = m_Head.GetIndex();

      if (head >= m_Tail)
      {
        if (idx >= head)
        {
          return;
        }
      }
      else
      {
        if (idx >= m_Size)
        {
          idx -= m_Size;

          if (idx >= head)
          {
            return;
          }
        }
      }

      idx += m_Offset;

      StormGenIndex queue_val = queue[idx];

      int val = queue_val.GetIndex();
      array[val].MessageInfo = value;
    }

		bool Advance(int gen, StormGenIndex * queue, StormMessageMegaContainer<T> * array)
		{
			// Advance the tail by one
			int idx = m_Tail;
			if (idx == m_Head.GetIndex())
			{
				return false;
			}

			if (idx == 0)
			{
				int new_cycles = (m_Cycles + 2) & 0xF;
				m_Cycles = new_cycles;
			}

			int new_tail = (idx + 1) % m_Size;
			idx += m_Offset;

			int val = queue[idx].GetIndex();

			// Free the queue slot
			StormGenIndex new_queue_val = StormGenIndex(-(m_Cycles), gen);
			queue[idx] = new_queue_val;

			StormGenIndex new_val = StormGenIndex(0, gen);
			array[val].HasData = new_val;

			m_Tail = new_tail;
			return true;
		}

		void Lock(int new_gen, StormGenIndex * queue, StormMessageMegaContainer<T> * array)
		{
			// Increase ALL THE GENs so that nothing can write to the queue
			while (true)
			{
				StormDoubleGenIndex old_index = m_Head;
				StormDoubleGenIndex new_index = StormDoubleGenIndex(old_index.GetIndex(), new_gen, old_index.GetGen2());

				if (std::atomic_compare_exchange_weak((std::atomic_int *)&m_Head.Raw, (int *)&old_index.Raw, (int)new_index.Raw))
				{
					break;
				}
			}

			// After this point, nothing should change
			for (int index = m_Offset; index < m_EndIndex; index++)
			{
				while (true)
				{
					StormGenIndex old_index = queue[index];
					StormGenIndex new_index = StormGenIndex(old_index.GetIndex(), new_gen);

					if (std::atomic_compare_exchange_weak((std::atomic_int *)&queue[index].Raw, (int *)&old_index.Raw, (int)new_index.Raw))
					{
						break;
					}
				}
			}

			for (int index = m_Offset; index < m_EndIndex; index++)
			{
				while (true)
				{
					StormGenIndex old_index = array[index].HasData;
					StormGenIndex new_index = StormGenIndex(old_index.GetIndex(), new_gen);

					if (std::atomic_compare_exchange_weak((std::atomic_int *)&array[index].HasData.Raw, (int *)&old_index.Raw, (int)new_index.Raw))
					{
						break;
					}
				}
			}

		}

		void Reset(int gen, StormGenIndex * queue, StormMessageMegaContainer<T> * array)
		{
			// This should be called after Lock() and everything has been drained from the queue
			m_Tail = 0;
			m_Head = StormDoubleGenIndex(0, gen, 1);
			m_Cycles = 1;

			for (int index = m_Offset; index < m_EndIndex; index++)
			{
				queue[index] = StormGenIndex(-1, gen);
			}
		}
	};
}

