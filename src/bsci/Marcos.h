#pragma once

#ifdef BSCI_EXPORTS

#define BSCI_API __declspec(dllexport)

#else

#define BSCI_API __declspec(dllimport)

#endif