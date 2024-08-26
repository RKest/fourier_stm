#pragma once

#include <array>
#include <cstdint>

namespace mi {

template<std::size_t Capacity>
class base_circular_buffer {
public:
	auto is_empty() -> bool {
		return read == write;
	}
	base_circular_buffer(UART_HandleTypeDef &uart_) :
			uart { uart_ } {
	}
protected:
	UART_HandleTypeDef &uart;

	std::array<uint8_t, Capacity> data{};
	uint8_t *read = data.begin();
	uint8_t *write = data.begin();

	void ptr_increment(uint8_t **ptr) {
		*ptr += 1;
		if (*ptr > data.begin() + Capacity - 1) {
			*ptr = data.begin();
		}

	}
};

template<std::size_t Capacity>
class rx_circular_buffer : public base_circular_buffer<Capacity> {
public:
	auto pop() -> uint8_t
	{
		uint8_t result = *this->read;
		__disable_irq();
		this->ptr_increment(&this->read);
		__enable_irq();
		return result;
	}

	void start_reception() {
		HAL_UART_Receive_IT(&this->uart, this->write, 1);
	}

	void receive(UART_HandleTypeDef &from) {
		if (&from != &this->uart)
			return;
		this->ptr_increment(&this->write);
		start_reception();
	}
};

template<std::size_t Capacity>
class tx_circular_buffer : public base_circular_buffer<Capacity> {
public:
	void push(uint8_t datum) {
		*this->write = datum;
		__disable_irq();
		this->ptr_increment(&this->write);
		__enable_irq();
	}

	void start_transmission() {
		HAL_UART_Transmit_IT(&this->uart, this->read, 1);
	}

	void transmit(UART_HandleTypeDef &from) {
		if (&from != &this->uart)
			return;
		this->ptr_increment(&this->read);
		if (this->is_empty() or is_busy())
			return;
		start_transmission();
	}

	auto is_busy() -> bool {
		return __HAL_UART_GET_FLAG(&this->uart, UART_FLAG_TXE) == RESET;
	}
};

}
