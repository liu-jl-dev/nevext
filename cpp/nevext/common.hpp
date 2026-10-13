#pragma once

namespace Nevext {
enum TokenType {
  Unexpected,
  EndOfFile,
  RawStr,
  QuotedStr,
  Sep,
};
enum Result {
  Success,
  Error,
  UnknownError,
  UnclosedStringError,
};

}  // namespace Nevext
