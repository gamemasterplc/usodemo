#pragma once

#define EXPORT_SYMBOL __attribute__((visibility("default"))) //Exports symbol to other USOs
#define ALIGN_SYMBOL(to) __attribute__((aligned((to)))) //Aligns symbol
#define SYMBOL_FUNC_CONSTRUCTOR __attribute__((constructor))
#define SYMBOL_FUNC_CONSTRUCTOR_PRIO(prio) __attribute__((constructor(prio)))
#define SYMBOL_FUNC_DESTRUCTOR __attribute__((destructor))
#define SYMBOL_FUNC_DESTRUCTOR_PRIO(prio) __attribute__((destructor(prio)))