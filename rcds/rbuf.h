#pragma once

#include <cstdint>

namespace rco {

	template <class T>
		class RingBuffer
		{
			public:
				RingBuffer(unsigned size): buffer_size(size), buffer_front(0), buffer_rear(0)
			{
				buffer = new T[size];
			}

				~RingBuffer()
				{
					delete [] buffer;
					buffer = nullptr;
				}

				inline bool empty() const
				{
					return buffer_front == buffer_rear;
				}

				inline bool full() const
				{
					return buffer_front == (buffer_rear + 1) % buffer_size;
				}

				bool push(const T& value)
				{
					if(full())
					{
						return false;
					}
					buffer[buffer_rear] = value;
					buffer_rear = (buffer_rear + 1) % buffer_size;
					return true;
				}

				bool push(const T* value)
				{
					if(full())
					{
						return false;
					}
					buffer[buffer_rear] = *value;
					buffer_rear = (buffer_rear + 1) % buffer_size;
					return true;
				}

				inline bool pop(T& value)
				{
					if(empty())
					{
						return false;
					}
					value = buffer[buffer_front];
					buffer_front = (buffer_front + 1) % buffer_size;
					return true;
				}
				inline unsigned int front()const
				{
					return buffer_front;
				}

				inline unsigned int rear()const
				{
					return buffer_rear;
				}
				inline unsigned int size()const
				{
					return buffer_size;
				}
			private:
				uint64_t buffer_size;	// 队列长度
				uint64_t buffer_front;	// 队列头部索引
				uint64_t buffer_rear;	// 队列尾部索引
				T*		 buffer;		// 数据缓冲区
		};
}
