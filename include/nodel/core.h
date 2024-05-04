#pragma once

#include <nodel/core/Object.h>
#include <nodel/parser/json.h>
#include <nodel/core/uri.h>
#include <nodel/core/bind.h>
#include <nodel/core/algo.h>
#include <nodel/support/logging.h>

/////////////////////////////////////////////////////////////////////////////
/// Nodel initialization macro
/// This macro must be instantiated before using nodel::* services.
/////////////////////////////////////////////////////////////////////////////
#define NODEL_INIT_CORE \
    NODEL_INIT_INTERNS; \
    NODEL_INIT_URI_SCHEMES;
