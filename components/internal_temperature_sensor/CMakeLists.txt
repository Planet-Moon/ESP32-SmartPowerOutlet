set(srcs "src/InternalTemperatureSensor.cpp")

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES driver)
