
struct webpa_topic
{
  string topic<>;
};

struct webpa_register_request
{
  int         prog_num;
  int         prog_vers;
  string      proto<>;
  string      host<>;
  webpa_topic topics<>;
};

struct webpa_register_response
{
  int id;
};

struct webpa_send_message_request
{
  opaque data<>;
};

struct webpa_send_message_response
{
  int ack;
};

program WEBPA_PROG
{
  version WEBPA_VERS
  {
    webpa_register_response     WEBPA_REGISTER(webpa_register_request)          = 1;
    webpa_send_message_response WEBPA_SEND_MESSAGE(webpa_send_message_request)  = 2;
  } = 1;
} = 0x20000008;
