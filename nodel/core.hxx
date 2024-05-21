/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <nodel/core/Object.hxx>
#include <nodel/parser/json.hxx>
#include <nodel/core/uri.hxx>
#include <nodel/core/bind.hxx>
#include <nodel/core/algo.hxx>
#include <nodel/support/logging.hxx>

/////////////////////////////////////////////////////////////////////////////
/// Nodel initialization macro
/// This macro must be instantiated before using nodel::* services.
/////////////////////////////////////////////////////////////////////////////
#define NODEL_INIT_CORE \
    NODEL_INIT_INTERNS; \
    NODEL_INIT_URI_SCHEMES;
