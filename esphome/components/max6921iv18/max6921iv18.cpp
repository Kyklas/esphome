#include "max6921iv18.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome
{
namespace max6921iv18
{

static unsigned char TubeDigit(unsigned char v);

static const char *TAG = "max6921iv18";

float MAX6921IV18Component::get_setup_priority() const
{
	return setup_priority::PROCESSOR;
}

void MAX6921IV18Component::spi_post_xfer(spi_transaction_t *trans)
{
	MAX6921IV18_Digit *digit = (MAX6921IV18_Digit *)trans->user;
	MAX6921IV18Component *max6921iv18comp = digit->max6921iv18comp;
	spi_transaction_t *trans_desc;
	digit->max6921iv18comp->latch_pin_->digital_write(true);

	if (max6921iv18comp->active)
	{
		esp_err_t ret;
		// reschedule the digit of current active digits
		spi_transaction_t *trans_desc =
			&max6921iv18comp->digits[max6921iv18comp->digits_active][digit->idx]
				 .xfer;

		ret =
			spi_device_queue_trans(digit->max6921iv18comp->spi, trans_desc, 0);
		// ESP_LOGD(TAG, "queue %d %d", digit->idx, ret);
	}
	digit->max6921iv18comp->latch_pin_->digital_write(false);
}

void MAX6921IV18Component::setup()
{
	ESP_LOGCONFIG(TAG, "Setting up MAX6921IV18...");

	this->enable_pin_->pin_mode(gpio::FLAG_OUTPUT);
	this->enable_pin_->digital_write(1);

	this->oe_pin_->pin_mode(gpio::FLAG_OUTPUT);
	this->oe_pin_->digital_write(1);

	this->latch_pin_->pin_mode(gpio::FLAG_OUTPUT);
	this->latch_pin_->digital_write(false);

	ESP_LOGCONFIG(TAG, " MAX6921IV18 SPI : %d %d", this->clock_pin_->get_pin(),
				  this->data_pin_->get_pin());

	esp_err_t ret;

	spi_bus_config_t buscfg = {
		.mosi_io_num = this->data_pin_->get_pin(),
		.miso_io_num = -1,
		.sclk_io_num = this->clock_pin_->get_pin(),
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = 3, // 3 bytes per xfer
	};
	spi_device_interface_config_t devcfg = {
		.mode = 0, // SPI mode 0
		.clock_speed_hz =
			24 * 9 * 100,	// 24 bits * 9 digit * 100Hz (1.1ms/xfer)
		.spics_io_num = -1, // CS pin
		.queue_size = 12,
		.post_cb =
			MAX6921IV18Component::spi_post_xfer, // Specify pre-transfer
												 // callback to handle D/C line
	};

	// Initialize the SPI bus
	ret = spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_DISABLED);
	ESP_ERROR_CHECK_WITHOUT_ABORT(ret);
	// Attach the LCD to the SPI bus
	ret = spi_bus_add_device(SPI3_HOST, &devcfg, &this->spi);
	ESP_ERROR_CHECK_WITHOUT_ABORT(ret);

	// Setup digits

	for (int g = 0; g < 2; g++)
	{
		for (int d = 0; d < 9; d++)
		{
			digits[g][d].max6921iv18comp = this;
			digits[g][d].idx = d;
			memset(&digits[g][d].xfer, 0, sizeof(digits[g][d].xfer));
			digits[g][d].xfer.length = 24;
			digits[g][d].xfer.flags = SPI_TRANS_USE_TXDATA;
			digits[g][d].xfer.user = &digits[g][d];
		}
	}

	digits_active = 0;

	if (this->writer_.has_value())
		(*this->writer_)(*this);
}

void MAX6921IV18Component::dump_config()
{
	ESP_LOGCONFIG(TAG, "MAX6921IV18: %s", __func__);
	ESP_LOGCONFIG(TAG, "  Intensity: %u", this->intensity_);
	LOG_PIN("  Enable Pin: ", this->enable_pin_);
}

void MAX6921IV18Component::display()
{
	ESP_LOGCONFIG(TAG, "MAX6921IV18: %s", __func__);
	digits_active ^= 1;

	if (!active)
	{
		active = 1;
		for (int d = 0; d < 9; d++)
		{
			esp_err_t ret;
			ret =
				spi_device_queue_trans(spi, &digits[digits_active][d].xfer, 0);
			ESP_ERROR_CHECK_WITHOUT_ABORT(ret);

			ESP_LOGD(TAG, "MAX6921IV18: %s %d %d %06x", __func__, digits_active,
					 d, (uint32_t)digits[digits_active][d].xfer.tx_buffer);
		}
	}
}

void MAX6921IV18Component::set_digit(uint8_t idx, uint8_t val, uint8_t seg)
{
	int g = !digits_active;
	MAX6921IV18_Digit *d = &digits[g][idx];
	d->xfer.tx_buffer = (void *)((uint32_t)(((TubeDigit(val) + seg) << 16) |
											((1 << idx + 8) & 0xFF00)));

	ESP_LOGD(TAG, "MAX6921IV18: %s %d %d %d", __func__, g, idx, val);
}

uint8_t MAX6921IV18Component::print(uint8_t start_pos, const char *str)
{
	ESP_LOGCONFIG(TAG, "MAX6921IV18: %s", __func__);

	for (int d = start_pos, c = 0; d < 8 && str[c] != '\0'; d++, c++)
	{
		int seg = 0;
		uint8_t chr = str[c];
		if (str[c + 1] == '.')
		{
			seg = 0x80; // dot, TODO move 7-seg def to new include
			c++;
		}
		set_digit(7 - d, chr, seg);
	}
	display();
	return 0;
}
uint8_t MAX6921IV18Component::print(const char *str)
{
	ESP_LOGCONFIG(TAG, "MAX6921IV18: %s", __func__);
	return this->print(0, str);
}
uint8_t MAX6921IV18Component::printf(uint8_t pos, const char *format, ...)
{
	ESP_LOGCONFIG(TAG, "MAX6921IV18: %s", __func__);
	return 0;
}
uint8_t MAX6921IV18Component::printf(const char *format, ...)
{
	ESP_LOGCONFIG(TAG, "MAX6921IV18: %s", __func__);

	va_list arg;
	va_start(arg, format);

	char buffer[10];
	int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
	if (ret > 0)
		return this->print(0,buffer);
	else
		return ret;
}

void MAX6921IV18Component::set_writer(max6921iv18_writer_t &&writer)
{
	ESP_LOGCONFIG(TAG, "MAX6921IV18: %s", __func__);
	this->writer_ = writer;
}
void MAX6921IV18Component::set_intensity(uint8_t intensity)
{
	ESP_LOGCONFIG(TAG, "MAX6921IV18: %s", __func__);
	this->intensity_ = intensity;
}

void MAX6921IV18Component::update()
{
	ESP_LOGD(TAG, "MAX6921IV18: %s", __func__);
	if (this->writer_.has_value())
		(*this->writer_)(*this);
	// this->display();
}

#ifdef USE_TIME
uint8_t MAX6921IV18Component::strftime(uint8_t pos, const char *format,
									   ESPTime time)
{
	char buffer[64];
	size_t ret = time.strftime(buffer, sizeof(buffer), format);
	if (ret > 0)
		return this->print(pos, buffer);
	return 0;
}
uint8_t MAX6921IV18Component::strftime(const char *format, ESPTime time)
{
	return this->strftime(0, format, time);
}
#endif

/*
 * Tube Digit
 */

#define FONT_DASH		/*0x02*/ 0x40 // dash
#define FONT_DEGREE		/*0xC6*/ 0x63 // degree
#define FONT_UNDERSCORE 0x08

#define SEG_TOP			(0x01)
#define SEG_MID			(0x40)
#define SEG_BOTTOM		(0x08)
#define SEG_TR			(0x02)
#define SEG_TL			(0x20)
#define SEG_BR			(0x04)
#define SEG_BL			(0x10)

#define IV18_DOT		0x80

#define IV18_0			0x3F /* 0 */
#define IV18_1			0x06 /* 1 */
#define IV18_2			0x5B /* 2 */
#define IV18_3			0x4F /* 3 */
#define IV18_4			0x66 /* 4 */
#define IV18_5			0x6D /* 5 */
#define IV18_6			0x7D /* 6 */
#define IV18_7			0x07 /* 7 */
#define IV18_8			0x7F /* 8 */
#define IV18_9			0x67 /* 9 */

#define IV18_a			0x77 // a 0
#define IV18_b			0x7C // b 1
#define IV18_c			0x58 // c 2
#define IV18_d			0x5E // d 3
#define IV18_e			(SEG_TOP | SEG_MID | SEG_BOTTOM | SEG_TL | SEG_BL)
#define IV18_f			0x71   // f 5
#define IV18_g			0x6F   // g 6
#define IV18_h			0x74   // h 7 //0x76
#define IV18_i			SEG_BR // i 8
#define IV18_j			0x1E   // j 9
#define IV18_k			0x75   // k 10
#define IV18_l			0x38   // l 11
#define IV18_m			0x37   // m 12
#define IV18_n			0x54   // n 13
#define IV18_o			0x5C   // o 14
#define IV18_p			0x73   // p 15
#define IV18_q			0xCF   // q 16
#define IV18_r			0x50   // r 17
#define IV18_s			0x6D   // s 18
#define IV18_t			0x78   // t 19
#define IV18_u			0x1C   // u 20
#define IV18_v			0x1C   // v 21
#define IV18_w			0x7E   //
#define IV18_x			0x76   // x 23
#define IV18_y			0x6E   // y 24
#define IV18_z			0x5B   // z 25

#define IV18_A			IV18_a
#define IV18_B			IV18_8
#define IV18_C			(SEG_TOP | SEG_BOTTOM | SEG_TL | SEG_BL)
#define IV18_D			IV18_d
#define IV18_E			(SEG_TOP | SEG_MID | SEG_BOTTOM | SEG_TL | SEG_BL)
#define IV18_F			IV18_f
#define IV18_G			IV18_g
#define IV18_H			(SEG_MID | SEG_TL | SEG_BL | SEG_TR | SEG_BR)
#define IV18_I			(SEG_TOP | SEG_BOTTOM | SEG_TL | SEG_BL)
#define IV18_J			IV18_j
#define IV18_K			IV18_k
#define IV18_L			(SEG_TL | SEG_BL | SEG_BOTTOM)
#define IV18_M			IV18_m
#define IV18_N			IV18_M
#define IV18_O			IV18_0
#define IV18_P			IV18_p
#define IV18_Q			IV18_q
#define IV18_R			IV18_r
#define IV18_S			IV18_s
#define IV18_T			(SEG_TOP | SEG_TL | SEG_BL)
#define IV18_U			(SEG_BOTTOM | SEG_TL | SEG_BL | SEG_TR | SEG_BR)
#define IV18_V			IV18_U
#define IV18_W			IV18_w
#define IV18_X			IV18_x
#define IV18_Y			IV18_y
#define IV18_Z			IV18_z

static const unsigned char alphatable[] = {
	/*0xEE*/ IV18_a, /* a 0*/
	/*0x3E*/ IV18_b, /* b 1*/
	/*0x1A*/ IV18_c, /* c 2*/
	/*0x7A*/ IV18_d, /* d 3*/
	/*0xDE*/ IV18_e, /* e 4*/
	/*0x8E*/ IV18_f, /* f 5*/
	/*0xF6*/ IV18_g, /* g 6*/
	/*0x2E*/ IV18_h,
	/* h 7*/		 // 0x74
	/*0x60*/ IV18_i, /* i 8*/
	/*0x78*/ IV18_j, /* j 9*/
	/*0xAE*/ IV18_k, // k 10
	/*0x1C*/ IV18_l, // l 11
	/*0xAA*/ IV18_m, // m 12
	/*0x2A*/ IV18_n, // n 13
	/*0x3A*/ IV18_o, // o 14
	/*0xCE*/ IV18_p, // p 15
	/*0xF3*/ IV18_q, // q 16
	/*0x0A*/ IV18_r, // r 17
	/*0xB6*/ IV18_s, // s 18
	/*0x1E*/ IV18_t, // t 19
	/*0x38*/ IV18_u, // u 20
	/*0x38*/ IV18_v, // v 21
	/*0xB8*/ IV18_w, // w 22
	/*0x6E*/ IV18_x, // x 23
	/*0x76*/ IV18_y, // y 24
	/*0xDA*/ IV18_z, // z 25
					 /* more */
};

static const unsigned char AlphaTable[] = {
	/*0xEE*/ IV18_A, /* a 0*/
	/*0x3E*/ IV18_B, /* b 1*/
	/*0x1A*/ IV18_C, /* c 2*/
	/*0x7A*/ IV18_D, /* d 3*/
	/*0xDE*/ IV18_E, /* e 4*/
	/*0x8E*/ IV18_F, /* f 5*/
	/*0xF6*/ IV18_G, /* g 6*/
	/*0x2E*/ IV18_H,
	/* h 7*/		 // 0x74
	/*0x60*/ IV18_I, /* i 8*/
	/*0x78*/ IV18_J, /* j 9*/
	/*0xAE*/ IV18_K, // k 10
	/*0x1C*/ IV18_L, // l 11
	/*0xAA*/ IV18_M, // m 12
	/*0x2A*/ IV18_N, // n 13
	/*0x3A*/ IV18_O, // o 14
	/*0xCE*/ IV18_P, // p 15
	/*0xF3*/ IV18_Q, // q 16
	/*0x0A*/ IV18_R, // r 17
	/*0xB6*/ IV18_S, // s 18
	/*0x1E*/ IV18_T, // t 19
	/*0x38*/ IV18_U, // u 20
	/*0x38*/ IV18_V, // v 21
	/*0xB8*/ IV18_W, // w 22
	/*0x6E*/ IV18_X, // x 23
	/*0x76*/ IV18_Y, // y 24
	/*0xDA*/ IV18_Z, // z 25
					 /* more */
};

static const unsigned char numbertable[] = {
	/*0xFC*/ IV18_0,
	/*0x60*/ IV18_1,
	/*0xDA*/ IV18_2,
	/*0xF2*/ IV18_3,
	/*0x66*/ IV18_4,
	/*0xB6*/ IV18_5,
	/*0xBE*/ IV18_6,
	/*0xE0*/ IV18_7,
	/*0xFE*/ IV18_8,
	/*0xE6*/ IV18_9,
};

unsigned char TubeDigit(unsigned char v)
{
	if ('a' <= v && v <= 'z')
	{
		return alphatable[v - 'a'];
	}
	if ('A' <= v && v <= 'Z')
	{
		return AlphaTable[v - 'A'];
	}
	if ('0' <= v && v <= '9')
	{
		return numbertable[v - '0'];
	}
	switch (v)
	{
	case '!':
		return SEG_TR | SEG_BR | IV18_DOT;
	case '"':
		return SEG_TL | SEG_TR;
	case '\'':
		return SEG_TL;
	case '`':
		return SEG_TR;
	case '@':
		return FONT_DEGREE;
	case '_':
		return FONT_UNDERSCORE;
	case '-':
		return FONT_DASH;
	}

	return 0;
}

} // namespace max6921iv18
} // namespace esphome