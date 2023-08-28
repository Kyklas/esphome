#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "esphome/core/defines.h"

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

#include "driver/spi_master.h"

namespace esphome
{
namespace max6921iv18
{

class MAX6921IV18Component;

using max6921iv18_writer_t = std::function<void(MAX6921IV18Component &)>;

// Help track spi xfer
typedef struct _MAX6921IV18_Digit
{
	spi_transaction_t xfer;
	MAX6921IV18Component *max6921iv18comp;
	uint8_t idx;
} MAX6921IV18_Digit;

class MAX6921IV18Component : public PollingComponent
{
  public:
	MAX6921IV18Component() = default;

	void setup() override;
	float get_setup_priority() const override;
	void dump_config() override;

	void set_enable_pin(GPIOPin *pin)
	{
		ESP_LOGD("IV18-H", __func__);
		enable_pin_ = pin;
	}
	void set_data_pin(InternalGPIOPin *pin)
	{
		ESP_LOGD("IV18-H", __func__);
		data_pin_ = pin;
	}
	void set_clock_pin(InternalGPIOPin *pin)
	{
		ESP_LOGD("IV18-H", __func__);
		clock_pin_ = pin;
	}
	void set_latch_pin(GPIOPin *pin)
	{
		ESP_LOGD("IV18-H", __func__);
		latch_pin_ = pin;
	}
	void set_oe_pin(GPIOPin *pin)
	{
		ESP_LOGD("IV18-H", __func__);
		oe_pin_ = pin;
	}

	void display();
	void set_intensity(uint8_t intensity);

	void update() override;

	void set_writer(max6921iv18_writer_t &&writer);

	/// Evaluate the printf-format and print the result at the given position.
	uint8_t printf(uint8_t pos, const char *format, ...)
		__attribute__((format(printf, 3, 4)));
	/// Evaluate the printf-format and print the result at position 0.
	uint8_t printf(const char *format, ...)
		__attribute__((format(printf, 2, 3)));

	/// Print `str` at the given position.
	uint8_t print(uint8_t pos, const char *str);
	/// Print `str` at position 0.
	uint8_t print(const char *str);

#ifdef USE_TIME
	/// Evaluate the strftime-format and print the result at the given position.
	uint8_t strftime(uint8_t pos, const char *format, time::ESPTime time)
		__attribute__((format(strftime, 3, 0)));

	/// Evaluate the strftime-format and print the result at position 0.
	uint8_t strftime(const char *format, time::ESPTime time)
		__attribute__((format(strftime, 2, 0)));
#endif

  protected:
	GPIOPin *enable_pin_;
	InternalGPIOPin *data_pin_;
	InternalGPIOPin *clock_pin_;
	GPIOPin *latch_pin_;
	GPIOPin *oe_pin_;

	uint8_t intensity_{15}; /// Intensity of the display from 0 to 15 (most)

	optional<max6921iv18_writer_t> writer_{};

	spi_device_handle_t spi;

	uint8_t active;
	MAX6921IV18_Digit digits[2][9];
	uint8_t digits_active;

	void set_digit(uint8_t idx, uint8_t val);

	static void spi_post_xfer(spi_transaction_t *trans);
};

} // namespace max6921iv18
} // namespace esphome