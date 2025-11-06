# Library providing code w.r.t. PEP's (data) tickets.

add_library(${PROJECT_NAME}Ticketinglib STATIC
  TicketingMessages.cpp TicketingMessages.hpp
  TicketingSerializers.cpp TicketingSerializers.hpp
)

target_link_libraries(${PROJECT_NAME}Ticketinglib
  ${PROJECT_NAME}RskPeplib
)
verify_headers_standalone(${PROJECT_NAME}Ticketinglib)
add_unit_tests(Ticketing)
