<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
<tagfile doxygen_version="1.9.8">
  <compound kind="file">
    <name>khc_socket_callback.h</name>
    <path>/home/runner/work/ebisu/ebisu/khc/include/</path>
    <filename>khc__socket__callback_8h.html</filename>
    <member kind="typedef">
      <type>enum khc_sock_code_t</type>
      <name>khc_sock_code_t</name>
      <anchorfile>khc__socket__callback_8h.html</anchorfile>
      <anchor>af497c50fbe34dcc5990d93c1e0c61909</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>khc_sock_code_t(*</type>
      <name>KHC_CB_SOCK_CONNECT</name>
      <anchorfile>khc__socket__callback_8h.html</anchorfile>
      <anchor>ae9552240ec64ad637e77cd63c3b65c65</anchor>
      <arglist>)(void *sock_ctx, const char *host, unsigned int port)</arglist>
    </member>
    <member kind="typedef">
      <type>khc_sock_code_t(*</type>
      <name>KHC_CB_SOCK_SEND</name>
      <anchorfile>khc__socket__callback_8h.html</anchorfile>
      <anchor>a96345c64c6bcec1711d2ddaac43224c2</anchor>
      <arglist>)(void *sock_ctx, const char *buffer, size_t length, size_t *out_sent_length)</arglist>
    </member>
    <member kind="typedef">
      <type>khc_sock_code_t(*</type>
      <name>KHC_CB_SOCK_RECV</name>
      <anchorfile>khc__socket__callback_8h.html</anchorfile>
      <anchor>a148cf4bd26b43909c9eab71f37b4cda6</anchor>
      <arglist>)(void *sock_ctx, char *buffer, size_t length_to_read, size_t *out_actual_length)</arglist>
    </member>
    <member kind="typedef">
      <type>khc_sock_code_t(*</type>
      <name>KHC_CB_SOCK_CLOSE</name>
      <anchorfile>khc__socket__callback_8h.html</anchorfile>
      <anchor>af54fb240d589fb1044188bd4a81ab78d</anchor>
      <arglist>)(void *sock_ctx)</arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>khc_sock_code_t</name>
      <anchorfile>khc__socket__callback_8h.html</anchorfile>
      <anchor>a13e4505815712d86adb95ab0ea867cf2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KHC_SOCK_OK</name>
      <anchorfile>khc__socket__callback_8h.html</anchorfile>
      <anchor>a13e4505815712d86adb95ab0ea867cf2aad80f9912fd8afc2b80d157cee2c679e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KHC_SOCK_FAIL</name>
      <anchorfile>khc__socket__callback_8h.html</anchorfile>
      <anchor>a13e4505815712d86adb95ab0ea867cf2a1cc5f5fa5f38faab5fa245a2e06e9d4a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KHC_SOCK_AGAIN</name>
      <anchorfile>khc__socket__callback_8h.html</anchorfile>
      <anchor>a13e4505815712d86adb95ab0ea867cf2a90a3f6acf289ae5d18feee3a67e69a1c</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>kii_task_callback.h</name>
    <path>/home/runner/work/ebisu/ebisu/kii/include/</path>
    <filename>kii__task__callback_8h.html</filename>
    <member kind="typedef">
      <type>enum kii_task_code_t</type>
      <name>kii_task_code_t</name>
      <anchorfile>kii__task__callback_8h.html</anchorfile>
      <anchor>aa7d48a35179c5c6885a5dd21f8fa57f2</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>enum kii_bool_t</type>
      <name>kii_bool_t</name>
      <anchorfile>kii__task__callback_8h.html</anchorfile>
      <anchor>a945d43709851462a1803721525df2b89</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>void *(*</type>
      <name>KII_TASK_ENTRY</name>
      <anchorfile>kii__task__callback_8h.html</anchorfile>
      <anchor>a803edec5a77dda169a7487f28eb7cf9a</anchor>
      <arglist>)(void *value)</arglist>
    </member>
    <member kind="typedef">
      <type>kii_task_code_t(*</type>
      <name>KII_CB_TASK_CREATE</name>
      <anchorfile>kii__task__callback_8h.html</anchorfile>
      <anchor>a0395b3c3b153a4ae039f9cbfb171c778</anchor>
      <arglist>)(const char *name, KII_TASK_ENTRY entry, void *entry_param, void *userdata)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>KII_CB_DELAY_MS</name>
      <anchorfile>kii__task__callback_8h.html</anchorfile>
      <anchor>aa8b275089808a99cff89acb673320273</anchor>
      <arglist>)(unsigned int msec, void *userdata)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>KII_CB_TASK_EXIT</name>
      <anchorfile>kii__task__callback_8h.html</anchorfile>
      <anchor>aa498370678fa8bb2e7aef2d18bc315b5</anchor>
      <arglist>)(void *task_info, void *userdata)</arglist>
    </member>
    <member kind="typedef">
      <type>kii_bool_t(*</type>
      <name>KII_CB_TASK_CONTINUE</name>
      <anchorfile>kii__task__callback_8h.html</anchorfile>
      <anchor>a66833479636e54ec6559fcb91c37263d</anchor>
      <arglist>)(void *task_info, void *userdata)</arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>kii_task_code_t</name>
      <anchorfile>kii__task__callback_8h.html</anchorfile>
      <anchor>a8cb8ae6e0e48ea307aa8481d5e85feb0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>kii_bool_t</name>
      <anchorfile>kii__task__callback_8h.html</anchorfile>
      <anchor>ae275605d20f9b00b1ead9cbc8e3b3283</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>tio_action_err_t</name>
    <filename>structtio__action__err__t.html</filename>
    <member kind="variable">
      <type>char</type>
      <name>err_message</name>
      <anchorfile>structtio__action__err__t.html</anchorfile>
      <anchor>aece44befd7f0b26314a54894a2fefff3</anchor>
      <arglist>[TIO_ACTION_ERR_MESSAGE_MAX_SIZE]</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>tio_action_result_data_t</name>
    <filename>structtio__action__result__data__t.html</filename>
    <member kind="variable">
      <type>char</type>
      <name>json</name>
      <anchorfile>structtio__action__result__data__t.html</anchorfile>
      <anchor>aa21434b0b8522eb4a49bca31d7c0fb5b</anchor>
      <arglist>[128]</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>tio_action_t</name>
    <filename>structtio__action__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>alias</name>
      <anchorfile>structtio__action__t.html</anchorfile>
      <anchor>a412290b7cbd2cd3838f66420ab1ba187</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>alias_length</name>
      <anchorfile>structtio__action__t.html</anchorfile>
      <anchor>ad376bb546c8147e2b0371c119932da2f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>action_name</name>
      <anchorfile>structtio__action__t.html</anchorfile>
      <anchor>a74dfdccea29e4ea983cb1aaf3f9cebd3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>action_name_length</name>
      <anchorfile>structtio__action__t.html</anchorfile>
      <anchor>a2e4dec6108670f0f3e1db8ff8e58feba</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>tio_action_value_t</type>
      <name>action_value</name>
      <anchorfile>structtio__action__t.html</anchorfile>
      <anchor>a069ab662c25522130cfae04320c69cff</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>tio_action_value_t</name>
    <filename>structtio__action__value__t.html</filename>
    <member kind="variable">
      <type>tio_data_type_t</type>
      <name>type</name>
      <anchorfile>structtio__action__value__t.html</anchorfile>
      <anchor>ae37aa5cf6246ce83845320168a92e832</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>long</type>
      <name>long_value</name>
      <anchorfile>structtio__action__value__t.html</anchorfile>
      <anchor>a6d17c1b2e1ad14fc98dcdf6d715b8538</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>double_value</name>
      <anchorfile>structtio__action__value__t.html</anchorfile>
      <anchor>af2132f619305f5535f068642d3064d87</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>tio_bool_t</type>
      <name>bool_value</name>
      <anchorfile>structtio__action__value__t.html</anchorfile>
      <anchor>ac75507595c7f721705c920fb3dc6c437</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>opaque_value</name>
      <anchorfile>structtio__action__value__t.html</anchorfile>
      <anchor>aa924dbe5c05e0df213710f356206dc4e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>union tio_action_value_t::@0</type>
      <name>param</name>
      <anchorfile>structtio__action__value__t.html</anchorfile>
      <anchor>a9487e84fbe922e3a1fade27909ef41a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>opaque_value_length</name>
      <anchorfile>structtio__action__value__t.html</anchorfile>
      <anchor>a2a32c6a02eff627a7892a17c5b1678bf</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>tio_handler_t</name>
    <filename>structtio__handler__t.html</filename>
  </compound>
  <compound kind="struct">
    <name>tio_handler_task_info_t</name>
    <filename>structtio__handler__task__info__t.html</filename>
    <member kind="variable">
      <type>kii_mqtt_error</type>
      <name>error</name>
      <anchorfile>structtio__handler__task__info__t.html</anchorfile>
      <anchor>a2b725069e1e91b1111e5ed37cce547a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>kii_mqtt_task_state</type>
      <name>task_state</name>
      <anchorfile>structtio__handler__task__info__t.html</anchorfile>
      <anchor>aec172492cb54a0cc922faf12b7a208d4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>tio_updater_t</name>
    <filename>structtio__updater__t.html</filename>
  </compound>
  <compound kind="page">
    <name>index</name>
    <title>tio</title>
    <filename>index.html</filename>
    <docanchor file="index.html" title="tio">tio_readme</docanchor>
    <docanchor file="index.html">task-callbacks</docanchor>
    <docanchor file="index.html">action-callback</docanchor>
    <docanchor file="index.html">execute-onboarding</docanchor>
    <docanchor file="index.html">size-callback</docanchor>
    <docanchor file="index.html">read-callback</docanchor>
    <docanchor file="index.html">asynchronous-task-management</docanchor>
  </compound>
</tagfile>
