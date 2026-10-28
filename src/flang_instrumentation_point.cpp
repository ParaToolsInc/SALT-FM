/* Copyright (C) 2025, ParaTools, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/


#include <string>
#include <sstream>
#include <iomanip>

#include "flang/Common/idioms.h"

#include "dprint.hpp"
#include "flang_instrumentation_point.hpp"
#include "flang_instrumentation_constants.hpp"
#include "selectfile.hpp"


using namespace std::string_literals;

std::string salt::fortran::InstrumentationPoint::typeString() const {
    switch (instrumentationType()) {
        case InstrumentationPointType::PROGRAM_BEGIN:
            return "PROGRAM_BEGIN"s;
        case InstrumentationPointType::PROGRAM_SETUP:
            return "PROGRAM_SETUP"s;
        case InstrumentationPointType::PROCEDURE_BEGIN:
            return "PROCEDURE_BEGIN"s;
        case InstrumentationPointType::PROCEDURE_END:
            return "PROCEDURE_END"s;
        case InstrumentationPointType::RETURN_STMT:
            return "RETURN_STMT"s;
        case InstrumentationPointType::IF_RETURN:
            return "IF_RETURN"s;
        case InstrumentationPointType::PROCEDURE_SKIPPED:
            return "PROCEDURE_SKIPPED"s;
        default:
            CRASH_NO_CASE;
    }
}

std::string salt::fortran::InstrumentationPoint::locationString() const {
    switch (location()) {
        case InstrumentationLocation::BEFORE:
            return "BEFORE"s;
        case InstrumentationLocation::AFTER:
            return "AFTER"s;
        case InstrumentationLocation::REPLACE:
            return "REPLACE"s;
        default:
            CRASH_NO_CASE;
    }
}

std::string salt::fortran::InstrumentationPoint::toString() const {
    std::stringstream ss;
    ss << line() << "\t";
    ss << locationString() << "\t";
    ss << typeString() << "\t";
    return ss.str();
}

std::string salt::fortran::InstrumentationPoint::instrumentationString(const InstrumentationMap &instMap,
                                                                       [[maybe_unused]] const std::string &lineText)
const {
    return instMap.at(instrumentationType());
}

std::string salt::fortran::ProgramBeginInstrumentationPoint::toString() const {
    std::stringstream ss;
    ss << InstrumentationPoint::toString();
    ss << "\"" << timerName() << "\"\t";
    return ss.str();
}

std::string salt::fortran::ProgramBeginInstrumentationPoint::instrumentationString(
    const InstrumentationMap &instMap, [[maybe_unused]] const std::string &lineText) const {
    return ReplacePhrase(InstrumentationPoint::instrumentationString(instMap, lineText),
                         SALT_FORTRAN_TIMER_NAME_TEMPLATE, timerName());
}

std::string salt::fortran::ProcedureBeginInstrumentationPoint::toString() const {
    std::stringstream ss;
    ss << InstrumentationPoint::toString();
    ss << "\"" << timerName() << "\"\t";
    return ss.str();
}

std::string salt::fortran::ProcedureBeginInstrumentationPoint::instrumentationString(
    const InstrumentationMap &instMap, [[maybe_unused]] const std::string &lineText) const {
    return ReplacePhrase(InstrumentationPoint::instrumentationString(instMap, lineText),
                         SALT_FORTRAN_TIMER_NAME_TEMPLATE, timerName());
}

// `pos` as line:column
static std::string positionString(const std::optional<salt::fortran::InputPosition> &pos) {
    return pos ? std::to_string(pos->line) + ":" + std::to_string(pos->column) : "-"s;
}

std::string salt::fortran::ReturnInstrumentationPoint::toString() const {
    std::stringstream ss;
    ss << InstrumentationPoint::toString();
    ss << positionString(start()) << "-" << positionString(end()) << "\t";
    ss << "next=" << positionString(next()) << "\t";
    if (label().has_value()) {
        ss << "label=" << label()->number << "\t";
    }
    return ss.str();
}

// The label goes on a `continue` statement of its own, so the stop-timer
// text may start with a directive or a comment, or be empty.
// F2018 R601: numeric labels occupy cols 1-5 (fixed form) followed by
// whitespace before the statement; free form accepts the same layout.
// Right-justify in a 5-col field so the line is valid for both forms.
std::string salt::fortran::ReturnInstrumentationPoint::instrumentationString(
    const InstrumentationMap &instMap, const std::string &lineText) const {
    const std::string text{InstrumentationPoint::instrumentationString(instMap, lineText)};
    if (!label().has_value()) {
        return text;
    }
    std::stringstream ss;
    ss << std::setw(5) << label()->number << " continue\n" << text;
    return ss.str();
}

std::string salt::fortran::IfReturnStmtInstrumentationPoint::toString() const {
    std::stringstream ss;
    ss << ReturnInstrumentationPoint::toString();
    ss << "return=" << positionString(returnStart()) << (returnSplit() ? " split" : "") << "\t";
    return ss.str();
}
