import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display
from esphome.const import CONF_ID, CONF_INTENSITY, CONF_DATA_PIN, CONF_CLOCK_PIN, CONF_ENABLE_PIN, CONF_LAMBDA, CONF_PAGES

DEPENDENCIES = []

max6921iv18_ns = cg.esphome_ns.namespace('max6921iv18')
MAX6921IV18Component = max6921iv18_ns.class_('MAX6921IV18Component', cg.PollingComponent, display.DisplayBuffer)
MAX6921IV18ComponentRef = MAX6921IV18Component.operator('ref')

CONF_MAX6921IV18 = 'max6921iv18'
CONF_LATCH_PIN = 'latch_pin'
CONF_OE_PIN = 'oe_pin'

CONFIG_SCHEMA = cv.All( 
    display.BASIC_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(MAX6921IV18Component),
            cv.Required(CONF_ENABLE_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_DATA_PIN): pins.internal_gpio_input_pin_schema,
            cv.Required(CONF_CLOCK_PIN): pins.internal_gpio_input_pin_schema,
            cv.Required(CONF_LATCH_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_OE_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_INTENSITY, default=15): cv.int_range(min=0, max=15),
            cv.Optional(CONF_PAGES): cv.All(
                cv.ensure_list(
                    {
                        cv.GenerateID(): cv.declare_id(display.DisplayPage),
                        cv.Required(CONF_LAMBDA): cv.lambda_,
                    }
                ),
                cv.Length(min=1),
            ),
        }
        ).extend(cv.polling_component_schema('1s')),
        # cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield display.register_display(var, config)

    enable_pin = yield cg.gpio_pin_expression(config[CONF_ENABLE_PIN])
    cg.add(var.set_enable_pin(enable_pin))
    data_pin = yield cg.gpio_pin_expression(config[CONF_DATA_PIN])
    cg.add(var.set_data_pin(data_pin))
    clock_pin = yield cg.gpio_pin_expression(config[CONF_CLOCK_PIN])
    cg.add(var.set_clock_pin(clock_pin))
    latch_pin = yield cg.gpio_pin_expression(config[CONF_LATCH_PIN])
    cg.add(var.set_latch_pin(latch_pin))
    oe_pin = yield cg.gpio_pin_expression(config[CONF_OE_PIN])
    cg.add(var.set_oe_pin(oe_pin))

    cg.add(var.set_intensity(config[CONF_INTENSITY]))

    if CONF_LAMBDA in config:
        lambda_ = yield cg.process_lambda(config[CONF_LAMBDA], [(MAX6921IV18ComponentRef, 'it')],
                                          return_type=cg.void)
        cg.add(var.set_writer(lambda_))

