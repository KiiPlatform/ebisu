<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
<tagfile doxygen_version="1.15.0">
  <compound kind="file">
    <name>kii_task_callback.h</name>
    <path>/home/runner/work/ebisu/ebisu/kii/include/</path>
    <filename>kii__task__callback_8h.html</filename>
    <member kind="typedef">
      <type>void(*</type>
      <name>KII_CB_TASK_EXIT</name>
      <anchorfile>kii__task__callback_8h.html</anchorfile>
      <anchor>aa498370678fa8bb2e7aef2d18bc315b5</anchor>
      <arglist>)(void *task_info, void *userdata)</arglist>
    </member>
    <member kind="typedef">
      <type>bool(*</type>
      <name>KII_CB_TASK_CONTINUE</name>
      <anchorfile>kii__task__callback_8h.html</anchorfile>
      <anchor>ad160d0af530d336062646f36cbad22f3</anchor>
      <arglist>)(void *task_info, void *userdata)</arglist>
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
      <type>int64_t</type>
      <name>long_value</name>
      <anchorfile>structtio__action__value__t.html</anchorfile>
      <anchor>af97df2666558442a1eebbfecbee78416</anchor>
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
      <type>bool</type>
      <name>bool_value</name>
      <anchorfile>structtio__action__value__t.html</anchorfile>
      <anchor>a3e8117ff74584f831d8b289fc2f04f05</anchor>
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
      <type>union tio_action_value_t::@062331171276345350325352277010343037332210371346</type>
      <name>param</name>
      <anchorfile>structtio__action__value__t.html</anchorfile>
      <anchor>ab23ef7c88bda5fde758021fbd98f4dd4</anchor>
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
    <docanchor file="index.html">mqtt-socket-notes</docanchor>
    <docanchor file="index.html">action-callback</docanchor>
    <docanchor file="index.html">session</docanchor>
    <docanchor file="index.html">clock</docanchor>
    <docanchor file="index.html">execute-onboarding</docanchor>
    <docanchor file="index.html">size-callback</docanchor>
    <docanchor file="index.html">read-callback</docanchor>
    <docanchor file="index.html">asynchronous-task-management</docanchor>
  </compound>
</tagfile>
