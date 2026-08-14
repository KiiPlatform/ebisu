<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
<tagfile doxygen_version="1.9.8">
  <compound kind="file">
    <name>kii.h</name>
    <path>include/</path>
    <filename>kii_8h.html</filename>
    <includes id="kii__task__callback_8h" name="kii_task_callback.h" local="yes" import="no" module="no" objc="no">kii_task_callback.h</includes>
    <class kind="struct">kii_bucket_t</class>
    <class kind="struct">kii_topic_t</class>
    <class kind="struct">kii_author_t</class>
    <class kind="struct">kii_mqtt_endpoint_t</class>
    <class kind="struct">kii_object_id_t</class>
    <class kind="struct">kii_installation_id_t</class>
    <class kind="struct">kii_t</class>
    <class kind="struct">kii_ti_firmware_version_t</class>
    <class kind="struct">kii_mqtt_task_info</class>
    <member kind="define">
      <type>#define</type>
      <name>KII_TASK_NAME_MQTT</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a48f058506a462a68bdf8fbd4c661accf</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>KII_AUTHOR_ID_MAX_SIZE</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a954de624a9c08e935ab8ded034ee0b95</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>KII_TOKEN_MAX_SIZE</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a430b4f23746528cf8103295f17ca33d6</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>KII_USERNAME_MAX_SIZE</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>aeb98949f480088c69d54de582576d7d6</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>KII_PASSWORD_MAX_SIZE</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>ad3f4dc3391175bb19d8a21309932701b</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>KII_TOPIC_MAX_SIZE</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>af24947bd7dc3c4d02fa145b3bd251718</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>KII_HOST_MAX_SIZE</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>ab298051f7ad14c1004b7ab4ca39887b5</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>KII_OBJECT_ID_MAX_SIZE</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a24f504a307f34b409bf85bb7bdc44433</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>KII_INSTALLATION_ID_MAX_SIZE</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a12590c5a89c0cd57281327c7ac92c38f</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>KII_APP_ID_MAX_SIZE</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a6c2c6a9018f3aa5b444861d557a7fefa</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>KII_APP_HOST_MAX_SIZE</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a45cdafdd717a2ecece4db96cbc04207d</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>KII_ETAG_MAX_SIZE</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a30bea1e5165e136c0dabfe2d6c37e9a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>KII_FIRMWARE_VERSION_MAX_SIZE</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a6be477b443a31b4ddc07540b762bbda3</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>size_t(*</type>
      <name>KII_CB_WRITE</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a1737de74c1494bd83e88734e14695c14</anchor>
      <arglist>)(char *buff, size_t size, void *userdata)</arglist>
    </member>
    <member kind="typedef">
      <type>size_t(*</type>
      <name>KII_CB_READ</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a8aff70d991d649449726c7967ade0f8d</anchor>
      <arglist>)(char *buffer, size_t size, void *userdata)</arglist>
    </member>
    <member kind="typedef">
      <type>enum kii_code_t</type>
      <name>kii_code_t</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a46c4d97d26b01fa9f5b6f97bca23531c</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>enum kii_scope_type_t</type>
      <name>kii_scope_type_t</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a3b61ff426f4e30331ac59b1ade036d5b</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct kii_bucket_t</type>
      <name>kii_bucket_t</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a6474402e02d6e65091a4c1ff788d5b7a</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct kii_topic_t</type>
      <name>kii_topic_t</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>aeaa6057c19679d23ddf086d5684bce8c</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct kii_author_t</type>
      <name>kii_author_t</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a1dec66c56a70b2a128ea8d80a409478b</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct kii_mqtt_endpoint_t</type>
      <name>kii_mqtt_endpoint_t</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a53a2e148b08f4402ae67e5d4900e7e5b</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct kii_object_id_t</type>
      <name>kii_object_id_t</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a796fdd785ce829d5fc0a87e2b0e1e318</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct kii_installation_id_t</type>
      <name>kii_installation_id_t</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>ab62fe7308c125c8b5643ba99c3626ed1</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>KII_PUSH_RECEIVED_CB</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>afeba27d97c92ad7a3a03374248eaeb14</anchor>
      <arglist>)(const char *message, size_t message_length, void *userdata)</arglist>
    </member>
    <member kind="typedef">
      <type>struct kii_t</type>
      <name>kii_t</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>ab1332877c811291e2a9693aaa5c15858</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>struct kii_ti_firmware_version_t</type>
      <name>kii_ti_firmware_version_t</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>af136309d5e5e98f8d912e85a8869d107</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>kii_code_t</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a79eab5c5f48e03121033d6291619c7de</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_ERR_OK</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a79eab5c5f48e03121033d6291619c7deaf063f9abcaa0e4e4f0ccc6b32b64e352</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_ERR_SOCK_CONNECT</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a79eab5c5f48e03121033d6291619c7dea1dd896dc55ff0c6093ed8820836ce0e7</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_ERR_SOCK_CLOSE</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a79eab5c5f48e03121033d6291619c7deab91c26aa92a78cb71de4bd77cbce7ea3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_ERR_SOCK_SEND</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a79eab5c5f48e03121033d6291619c7deaf2e6ff2f4cd85fb9e2ae4105adf620d6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_ERR_SOCK_RECV</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a79eab5c5f48e03121033d6291619c7deae1968e52c59cdef7b301b4ec139147b3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_ERR_HEADER_CALLBACK</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a79eab5c5f48e03121033d6291619c7dead91d57d51f1b7eeb3f29193acaddf6d9</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_ERR_WRITE_CALLBACK</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a79eab5c5f48e03121033d6291619c7dea2a796e5d50e605c3b3fa095be18f5c67</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_ERR_ALLOCATION</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a79eab5c5f48e03121033d6291619c7dead839e55c9df8a32740642cb33b802d9c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_ERR_TOO_LARGE_DATA</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a79eab5c5f48e03121033d6291619c7dea7d0fc4db434b704d555df6eeb452d7d7</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_ERR_RESP_STATUS</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a79eab5c5f48e03121033d6291619c7deac9472b83ca9b65665e8b92b039033567</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_ERR_PARSE_JSON</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a79eab5c5f48e03121033d6291619c7dea95be3c52f09a17ab37cc5026f4f74c21</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_ERR_FAIL</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a79eab5c5f48e03121033d6291619c7dea8fc54c71516a569079c207f4de28eb41</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>kii_scope_type_t</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>ad291064c8d47469d82a581e06a92d1dc</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_SCOPE_APP</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>ad291064c8d47469d82a581e06a92d1dcaf4ac24e2bbddbc00ab0aa12f151fffd6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_SCOPE_USER</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>ad291064c8d47469d82a581e06a92d1dcab75f6e39aeef01d768a2146661443dfd</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_SCOPE_GROUP</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>ad291064c8d47469d82a581e06a92d1dca9768b6b29b6533fb18705d6ba256b377</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_SCOPE_THING</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>ad291064c8d47469d82a581e06a92d1dca7b6b4b2b3cfb594b1a5e68dbbe8a9390</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>kii_mqtt_task_state</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>afbdebbbeb8812da258f2c985a2b10db3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_MQTT_ST_INSTALL_PUSH</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>afbdebbbeb8812da258f2c985a2b10db3a732682069159c445921e286c056aa6bb</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_MQTT_ST_GET_ENDPOINT</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>afbdebbbeb8812da258f2c985a2b10db3a10325842254cc0f24ec27ffb9798617d</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_MQTT_ST_SOCK_CONNECT</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>afbdebbbeb8812da258f2c985a2b10db3a897362005afd9ec3e6ce2fe835270458</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_MQTT_ST_SEND_CONNECT</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>afbdebbbeb8812da258f2c985a2b10db3aac800502452b8784f558e7cc2ee6cd54</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_MQTT_ST_RECV_CONNACK</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>afbdebbbeb8812da258f2c985a2b10db3a4a0552111077f56357b67824ed8728d8</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_MQTT_ST_SEND_SUBSCRIBE</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>afbdebbbeb8812da258f2c985a2b10db3af0fa7e3699a3b84da36f56c3461c9c2c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_MQTT_ST_RECV_SUBACK</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>afbdebbbeb8812da258f2c985a2b10db3ac64f894e9fa5128616926f5138f182e2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_MQTT_ST_RECV_READY</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>afbdebbbeb8812da258f2c985a2b10db3ac54a5a76afc0fd08cb39eb09bf600d44</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_MQTT_ST_RECV_MSG</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>afbdebbbeb8812da258f2c985a2b10db3a133dbec03e4e77ddfd4df2f1abc988b3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_MQTT_ST_SEND_PINGREQ</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>afbdebbbeb8812da258f2c985a2b10db3ae28e87c388e298b2d6f575c0417b8a52</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_MQTT_ST_RECONNECT</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>afbdebbbeb8812da258f2c985a2b10db3adec6a548d8ad9577dc54cc1b3dda615f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_MQTT_ST_ERR_EXIT</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>afbdebbbeb8812da258f2c985a2b10db3acddcf8a4f850063b9fe95f0f88800b24</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_MQTT_ST_DISCONTINUED</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>afbdebbbeb8812da258f2c985a2b10db3a82d610abc8b8d60b8aaabaa5c14df92d</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>kii_mqtt_error</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>ace11a8cf8764373a85c96643195289b2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_MQTT_ERR_OK</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>ace11a8cf8764373a85c96643195289b2acbcfb4685d031175132599b1f925b798</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_MQTT_ERR_INSTALLATION</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>ace11a8cf8764373a85c96643195289b2afde8aeb9b4525616e3c461412e54d726</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_MQTT_ERR_GET_ENDPOINT</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>ace11a8cf8764373a85c96643195289b2aa32ccee9cf4b58d75cdeb92b36eee6a8</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>KII_MQTT_ERR_INSUFFICIENT_BUFF</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>ace11a8cf8764373a85c96643195289b2a9cdad92c2105b2fc05f9264be83d59a9</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>kii_init</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a7ba3ce1d86f6048b7b3fefba36056410</anchor>
      <arglist>(kii_t *kii)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>kii_set_site</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>ac192a729b19da5d4feb1d288f42560a9</anchor>
      <arglist>(kii_t *kii, const char *site)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>kii_set_app_id</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a9bf8c483fe349d5d8cb7f417f8832b90</anchor>
      <arglist>(kii_t *kii, const char *app_id)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>kii_enable_insecure_http</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a1ac5ab10b44de5397b7227e4e2aaca4e</anchor>
      <arglist>(kii_t *kii, kii_bool_t enable_insecure_http)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>kii_enable_insecure_mqtt</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a1d4b321dd8a70434bae5b07a70c14fba</anchor>
      <arglist>(kii_t *kii, kii_bool_t enable_insecure_mqtt)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_auth_thing</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a128f23578c2e848e6d20e05faae9b29d</anchor>
      <arglist>(kii_t *kii, const char *vendor_thing_id, const char *password)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_register_thing</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a113672afcdae61484731c80a3b7bc17e</anchor>
      <arglist>(kii_t *kii, const char *vendor_thing_id, const char *thing_type, const char *password)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_post_object</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>ac3ecf806fb18b0063b50291b8131dcfc</anchor>
      <arglist>(kii_t *kii, const kii_bucket_t *bucket, const char *object_data, const char *object_content_type, kii_object_id_t *out_object_id)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_put_object</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>ae6940b66d1a215880f76b03ed0a74ec3</anchor>
      <arglist>(kii_t *kii, const kii_bucket_t *bucket, const char *object_id, const char *object_data, const char *object_content_type, const char *opt_etag)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_patch_object</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a63e7f18308e41bf8d558530cb52e55ee</anchor>
      <arglist>(kii_t *kii, const kii_bucket_t *bucket, const char *object_id, const char *patch_data, const char *opt_etag)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_delete_object</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a0243b3014379f29668584d078c7a7eb2</anchor>
      <arglist>(kii_t *kii, const kii_bucket_t *bucket, const char *object_id)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_get_object</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a2554eb45402af772c9d82980dba65167</anchor>
      <arglist>(kii_t *kii, const kii_bucket_t *bucket, const char *object_id)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_upload_object_body</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a818b618ecdcb5fc08dbad0462d37f639</anchor>
      <arglist>(kii_t *kii, const kii_bucket_t *bucket, const char *object_id, const char *body_content_type, const KII_CB_READ read_cb, void *userdata)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_download_object_body</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>ae7dc8837d32daa841f297784a6d15499</anchor>
      <arglist>(kii_t *kii, const kii_bucket_t *bucket, const char *object_id, const KII_CB_WRITE write_cb, void *userdata)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_subscribe_bucket</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>ad61062adaf499613b80bbf4e7cfd526f</anchor>
      <arglist>(kii_t *kii, const kii_bucket_t *bucket)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_unsubscribe_bucket</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a59e62cdf94dc9a34a75f40ce24b9ab19</anchor>
      <arglist>(kii_t *kii, const kii_bucket_t *bucket)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_subscribe_topic</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a01bef79a9e335923fc3817c1b60f1580</anchor>
      <arglist>(kii_t *kii, const kii_topic_t *topic)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_unsubscribe_topic</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>afebea79385b02f120416efb5798d57aa</anchor>
      <arglist>(kii_t *kii, const kii_topic_t *topic)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_put_topic</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>afaabbe9301b5978deea3980ee5f8c076</anchor>
      <arglist>(kii_t *kii, const kii_topic_t *topic)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_delete_topic</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>adb8c2b15d4e488d36eabaa91cba61b6f</anchor>
      <arglist>(kii_t *kii, const kii_topic_t *topic)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_install_push</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a53495a9269f8822e7f6e519ebb40d2f3</anchor>
      <arglist>(kii_t *kii, kii_bool_t development, kii_installation_id_t *out_installation_id)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_get_mqtt_endpoint</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a9cde2654b8a652bb01bb5952faf0e538</anchor>
      <arglist>(kii_t *kii, const char *installation_id, kii_mqtt_endpoint_t *endpoint)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_start_push_task</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a2106af24a260dc351b0c17e388a4bd71</anchor>
      <arglist>(kii_t *kii, unsigned int keep_alive_interval_sec, KII_PUSH_RECEIVED_CB cb_push, void *userdata)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_execute_server_code_version</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a88de31a3b85076df9a58727d2ab63d7f</anchor>
      <arglist>(kii_t *kii, const char *version, const char *endpoint_name, const char *params)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_execute_server_code</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a54c7ef600d31e8e1f6826dbaa2742b04</anchor>
      <arglist>(kii_t *kii, const char *endpoint_name, const char *params)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_ti_onboard</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>adaf6e5b9a952127cacd4cbf2e911039c</anchor>
      <arglist>(kii_t *kii, const char *vendor_thing_id, const char *password, const char *thing_type, const char *firmware_version, const char *layout_position, const char *thing_properties)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_ti_put_firmware_version</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>aaab1b9126bf3ef38f62f35b064846d1c</anchor>
      <arglist>(kii_t *kii, const char *firmware_version)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_ti_get_firmware_version</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>af3a4c97bedabb6990a1811deb46fba62</anchor>
      <arglist>(kii_t *kii, kii_ti_firmware_version_t *version)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_ti_put_state</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a6e04e62002ba5192abf39152c499a627</anchor>
      <arglist>(kii_t *kii, KII_CB_READ state_read_cb, void *state_read_cb_data, const char *opt_content_type, const char *opt_content_encoding, const char *opt_normalizer_host)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_ti_put_bulk_states</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a96088b3f8df966a766f98c979ca6a6de</anchor>
      <arglist>(kii_t *kii, KII_CB_READ state_read_cb, void *state_read_cb_data, const char *opt_content_type, const char *opt_content_encoding, const char *opt_normalizer_host)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_ti_patch_state</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a5bf7d72c2b9bb9eddf68a7ac49aa4540</anchor>
      <arglist>(kii_t *kii, KII_CB_READ state_read_cb, void *state_read_cb_data, const char *opt_content_type, const char *opt_content_encoding, const char *opt_normalizer_host)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_ti_patch_bulk_states</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>ab70224f79201e72bee4aeb8db93939cc</anchor>
      <arglist>(kii_t *kii, KII_CB_READ state_read_cb, void *state_read_cb_data, const char *opt_content_type, const char *opt_content_encoding, const char *opt_normalizer_host)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_api_call_start</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a848b722808c9a43c9d66a2aca09a9c04</anchor>
      <arglist>(kii_t *kii, const char *http_method, const char *resource_path, const char *content_type, kii_bool_t set_authentication_header)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_api_call_append_body</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a630b808717fedeb41a643b692617bc39</anchor>
      <arglist>(kii_t *kii, const char *chunk, size_t chunk_size)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_api_call_append_header</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>aa9a202d325f0d5021d4d47ee6e78125c</anchor>
      <arglist>(kii_t *kii, const char *key, const char *value)</arglist>
    </member>
    <member kind="function">
      <type>kii_code_t</type>
      <name>kii_api_call_run</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>aa6640658ca65a4a54319b8ff474450c6</anchor>
      <arglist>(kii_t *kii)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>kii_set_buff</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>ae7e91d48a26389dc911d799e41fa149d</anchor>
      <arglist>(kii_t *kii, char *buff, size_t buff_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>kii_set_stream_buff</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>ac7fb349278a3ae5104386a9b14a78a99</anchor>
      <arglist>(kii_t *kii, char *buff, size_t buff_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>kii_set_resp_header_buff</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>afeac6cc4848254cf840956e5cee04bcd</anchor>
      <arglist>(kii_t *kii, char *buff, size_t buff_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>kii_set_cb_http_sock_connect</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a2da9f4270de88d128d37e6ebde08540b</anchor>
      <arglist>(kii_t *kii, KHC_CB_SOCK_CONNECT cb_connect, void *userdata)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>kii_set_cb_http_sock_send</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>ae2b2ac2283f285b5f2771731a80ecdfb</anchor>
      <arglist>(kii_t *kii, KHC_CB_SOCK_SEND cb_send, void *userdata)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>kii_set_cb_http_sock_recv</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a9e3e9ce59e18d212b5a961ca8650c2ad</anchor>
      <arglist>(kii_t *kii, KHC_CB_SOCK_RECV cb_recv, void *userdata)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>kii_set_cb_http_sock_close</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a491beb9abda4a3e26dbffb6f10b37f4d</anchor>
      <arglist>(kii_t *kii, KHC_CB_SOCK_CLOSE cb_close, void *userdata)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>kii_set_mqtt_buff</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>adb5bef6d077b15fca3195ef7d39681cd</anchor>
      <arglist>(kii_t *kii, char *buff, size_t buff_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>kii_set_cb_mqtt_sock_connect</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a6125a1326008370393f502e23cc43781</anchor>
      <arglist>(kii_t *kii, KHC_CB_SOCK_CONNECT cb_connect, void *userdata)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>kii_set_cb_mqtt_sock_send</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a92d3014f87739a42c28b592c3e9d323e</anchor>
      <arglist>(kii_t *kii, KHC_CB_SOCK_SEND cb_send, void *userdata)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>kii_set_cb_mqtt_sock_recv</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a53f75c9d2fab7fc43c5e0478864727e9</anchor>
      <arglist>(kii_t *kii, KHC_CB_SOCK_RECV cb_recv, void *userdata)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>kii_set_cb_mqtt_sock_close</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a4c1ba2c47261d6c1709298d435a8e77f</anchor>
      <arglist>(kii_t *kii, KHC_CB_SOCK_CLOSE cb_close, void *userdata)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>kii_set_mqtt_to_sock_recv</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>aaf9cdb339d7b2f4a598ef2373b4ac438</anchor>
      <arglist>(kii_t *kii, unsigned int to_sock_recv_sec)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>kii_set_mqtt_to_sock_send</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>aeec93d3842f8340dc157260ae3637f21</anchor>
      <arglist>(kii_t *kii, unsigned int to_sock_send_sec)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>kii_set_cb_task_create</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a0544a628a2fbc798e4d2fa67e142c491</anchor>
      <arglist>(kii_t *kii, KII_CB_TASK_CREATE cb_task_create, void *userdata)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>kii_set_cb_task_continue</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a308bbd95790fad608a661d8e5823c1d8</anchor>
      <arglist>(kii_t *kii, KII_CB_TASK_CONTINUE cb_continue, void *userdata)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>kii_set_cb_task_exit</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a5d4c80bb655870e967d5a90500bcda08</anchor>
      <arglist>(kii_t *kii, KII_CB_TASK_EXIT cb_exit, void *userdata)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>kii_set_cb_delay_ms</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a279015373defa4b13d772568a2be6fea</anchor>
      <arglist>(kii_t *kii, KII_CB_DELAY_MS cb_delay_ms, void *userdata)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>kii_set_json_parser_resource</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>ace208de016c2792d00a43374638bb34c</anchor>
      <arglist>(kii_t *kii, jkii_resource_t *resource)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>kii_set_cb_json_parser_resource</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a644e3299e543b6dc1d32f8fd84cc6437</anchor>
      <arglist>(kii_t *kii, JKII_CB_RESOURCE_ALLOC cb_alloc, JKII_CB_RESOURCE_FREE cb_free)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>kii_set_cb_slist_resource</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>af8d81eaeba5dfa0aceff7347e8f7419a</anchor>
      <arglist>(kii_t *kii, KHC_CB_SLIST_ALLOC cb_alloc, KHC_CB_SLIST_FREE cb_free, void *cb_alloc_data, void *cb_free_data)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>kii_get_etag</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a51c376c5e496e52aec2375a3ff70a885</anchor>
      <arglist>(kii_t *kii)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>kii_get_resp_status</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a5cfa2d3b02d827372f3f199715687b96</anchor>
      <arglist>(kii_t *kii)</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>kii_get_resp_body_length</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>acc772d67b6d4f9cf53e0c5ea864ceb5d</anchor>
      <arglist>(kii_t *kii)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>kii_set_use_m_0_header_flag</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a9f566e30857449a1899d80c91bfcdd6b</anchor>
      <arglist>(kii_t *kii, kii_bool_t flag)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>kii_cancel_request_being_prepared</name>
      <anchorfile>kii_8h.html</anchorfile>
      <anchor>a0709f4778e23ff51645274ec79fa7efa</anchor>
      <arglist>(kii_t *kii)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>kii_task_callback.h</name>
    <path>include/</path>
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
    <name>kii_author_t</name>
    <filename>structkii__author__t.html</filename>
    <member kind="variable">
      <type>char</type>
      <name>author_id</name>
      <anchorfile>structkii__author__t.html</anchorfile>
      <anchor>a7ef55277d3fc92e56bf79e3f1838891e</anchor>
      <arglist>[KII_AUTHOR_ID_MAX_SIZE]</arglist>
    </member>
    <member kind="variable">
      <type>char</type>
      <name>access_token</name>
      <anchorfile>structkii__author__t.html</anchorfile>
      <anchor>aecbad5bf0ac42fe9c08a079ae4df2279</anchor>
      <arglist>[KII_TOKEN_MAX_SIZE]</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>kii_bucket_t</name>
    <filename>structkii__bucket__t.html</filename>
    <member kind="variable">
      <type>kii_scope_type_t</type>
      <name>scope</name>
      <anchorfile>structkii__bucket__t.html</anchorfile>
      <anchor>aba6f08f67d2f5b57aaedceb75dfc0528</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>scope_id</name>
      <anchorfile>structkii__bucket__t.html</anchorfile>
      <anchor>af5cdb5e5d4558d80a4196d7c91bc81be</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>bucket_name</name>
      <anchorfile>structkii__bucket__t.html</anchorfile>
      <anchor>a017e05bd9ec6a26eeb4b9099da60f858</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>kii_installation_id_t</name>
    <filename>structkii__installation__id__t.html</filename>
    <member kind="variable">
      <type>char</type>
      <name>id</name>
      <anchorfile>structkii__installation__id__t.html</anchorfile>
      <anchor>a4e84b83a1f4cc5438e8832bebad99e30</anchor>
      <arglist>[KII_INSTALLATION_ID_MAX_SIZE]</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>kii_mqtt_endpoint_t</name>
    <filename>structkii__mqtt__endpoint__t.html</filename>
    <member kind="variable">
      <type>char</type>
      <name>username</name>
      <anchorfile>structkii__mqtt__endpoint__t.html</anchorfile>
      <anchor>aec4ade1f997ee17af5360af5611f0c8f</anchor>
      <arglist>[KII_USERNAME_MAX_SIZE]</arglist>
    </member>
    <member kind="variable">
      <type>char</type>
      <name>password</name>
      <anchorfile>structkii__mqtt__endpoint__t.html</anchorfile>
      <anchor>a1ff8d308170fe4d683839e85ab9d6474</anchor>
      <arglist>[KII_PASSWORD_MAX_SIZE]</arglist>
    </member>
    <member kind="variable">
      <type>char</type>
      <name>topic</name>
      <anchorfile>structkii__mqtt__endpoint__t.html</anchorfile>
      <anchor>a541920b531df8bb20f58e1dd7c0a3c81</anchor>
      <arglist>[KII_TOPIC_MAX_SIZE]</arglist>
    </member>
    <member kind="variable">
      <type>char</type>
      <name>host</name>
      <anchorfile>structkii__mqtt__endpoint__t.html</anchorfile>
      <anchor>a27ff0eebfcbf232e0f132e3002483994</anchor>
      <arglist>[KII_HOST_MAX_SIZE]</arglist>
    </member>
    <member kind="variable">
      <type>unsigned int</type>
      <name>port_tcp</name>
      <anchorfile>structkii__mqtt__endpoint__t.html</anchorfile>
      <anchor>a9906ff89550cb9dad3e5a05eb1829b88</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned int</type>
      <name>port_ssl</name>
      <anchorfile>structkii__mqtt__endpoint__t.html</anchorfile>
      <anchor>a26db4f119fc76373a41e1137d63f629b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned long</type>
      <name>ttl</name>
      <anchorfile>structkii__mqtt__endpoint__t.html</anchorfile>
      <anchor>ad07b60aae4d353b09eac993a725f353e</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>kii_mqtt_task_info</name>
    <filename>structkii__mqtt__task__info.html</filename>
    <member kind="variable">
      <type>kii_mqtt_error</type>
      <name>error</name>
      <anchorfile>structkii__mqtt__task__info.html</anchorfile>
      <anchor>a92ee0b673e721c1deb760bb6853ec42e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>kii_mqtt_task_state</type>
      <name>task_state</name>
      <anchorfile>structkii__mqtt__task__info.html</anchorfile>
      <anchor>a10dd846ce66170de1a124dc32cdc46b8</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>kii_object_id_t</name>
    <filename>structkii__object__id__t.html</filename>
    <member kind="variable">
      <type>char</type>
      <name>id</name>
      <anchorfile>structkii__object__id__t.html</anchorfile>
      <anchor>a19d17e0013f10a89fab4efbea6b2153a</anchor>
      <arglist>[KII_OBJECT_ID_MAX_SIZE]</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>kii_t</name>
    <filename>structkii__t.html</filename>
  </compound>
  <compound kind="struct">
    <name>kii_ti_firmware_version_t</name>
    <filename>structkii__ti__firmware__version__t.html</filename>
    <member kind="variable">
      <type>char</type>
      <name>firmware_version</name>
      <anchorfile>structkii__ti__firmware__version__t.html</anchorfile>
      <anchor>a12e3389e2ec3c812ebd79e37e222bad4</anchor>
      <arglist>[KII_FIRMWARE_VERSION_MAX_SIZE]</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>kii_topic_t</name>
    <filename>structkii__topic__t.html</filename>
    <member kind="variable">
      <type>kii_scope_type_t</type>
      <name>scope</name>
      <anchorfile>structkii__topic__t.html</anchorfile>
      <anchor>a2967810519663f7e6de38f71098d9709</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>scope_id</name>
      <anchorfile>structkii__topic__t.html</anchorfile>
      <anchor>a194b3a3e1aefe07ba9662da0b4667c07</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>topic_name</name>
      <anchorfile>structkii__topic__t.html</anchorfile>
      <anchor>a2ea6785013916b715b151da705a5d54a</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="page">
    <name>index</name>
    <title>Build SDK</title>
    <filename>index.html</filename>
    <docanchor file="index.html" title="Build SDK">kii_readme</docanchor>
  </compound>
</tagfile>
