// Helper macros for dealing with CelValues.
//
// Never include this file in another header, as macros are declared in
// the global scope.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_CEL_MACROS_H_
#define THIRD_PARTY_CEL_CPP_COMMON_CEL_MACROS_H_

/** Returns the CelValue immediately if it represents an Error or Unknown. */
#define RETURN_IF_NOT_VALUE(expr)              \
  do {                                         \
    auto return_if_not_value_value = (expr);   \
    if (!return_if_not_value_value.is_value()) \
      return return_if_not_value_value;        \
  } while (false)

/** Helper macro to return a status eagerly, if it represents an error. */
#define RETURN_IF_STATUS_ERROR(expr)                                     \
  do {                                                                   \
    auto return_if_status_error_status = (expr);                         \
    if (return_if_status_error_status.code() != ::google::rpc::Code::OK) \
      return return_if_status_error_status;                              \
  } while (false)

#endif  // THIRD_PARTY_CEL_CPP_COMMON_CEL_MACROS_H_
