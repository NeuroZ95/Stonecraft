#include "../../tools/logger/logger.h"

#ifdef _MSC_VER
#pragma warning(disable: 4267 4244 4018 4146)
#endif

// Макрос ENET_IMPLEMENTATION должен быть объявлен строго в одном файле перед подключением header
#define ENET_IMPLEMENTATION
#include <enet.h>