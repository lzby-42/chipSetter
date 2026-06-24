TEMPLATE = subdirs
CONFIG += c++11
SUBDIRS = test_hardware_config test_motor_axis test_process_manager

test_hardware_config.file = test_hardware_config.pro
test_motor_axis.file = test_motor_axis.pro
test_process_manager.file = test_process_manager.pro
