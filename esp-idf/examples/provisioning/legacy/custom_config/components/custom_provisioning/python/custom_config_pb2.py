# -*- coding: utf-8 -*-
# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: custom_config.proto
"""Generated protocol buffer code."""
from google.protobuf.internal import builder as _builder
from google.protobuf import descriptor as _descriptor
from google.protobuf import descriptor_pool as _descriptor_pool
from google.protobuf import symbol_database as _symbol_database
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()




DESCRIPTOR = _descriptor_pool.Default().AddSerializedFile(b'\n\x13\x63ustom_config.proto\"4\n\x13\x43ustomConfigRequest\x12\x0c\n\x04info\x18\x01 \x01(\t\x12\x0f\n\x07version\x18\x02 \x01(\x05\"J\n\x14\x43ustomConfigResponse\x12#\n\x06status\x18\x01 \x01(\x0e\x32\x13.CustomConfigStatus\x12\r\n\x05\x64ummy\x18\x02 \x01(\x05*7\n\x12\x43ustomConfigStatus\x12\x11\n\rConfigSuccess\x10\x00\x12\x0e\n\nConfigFail\x10\x01\x62\x06proto3')

_builder.BuildMessageAndEnumDescriptors(DESCRIPTOR, globals())
_builder.BuildTopDescriptorsAndMessages(DESCRIPTOR, 'custom_config_pb2', globals())
if _descriptor._USE_C_DESCRIPTORS == False:

  DESCRIPTOR._options = None
  _CUSTOMCONFIGSTATUS._serialized_start=153
  _CUSTOMCONFIGSTATUS._serialized_end=208
  _CUSTOMCONFIGREQUEST._serialized_start=23
  _CUSTOMCONFIGREQUEST._serialized_end=75
  _CUSTOMCONFIGRESPONSE._serialized_start=77
  _CUSTOMCONFIGRESPONSE._serialized_end=151
# @@protoc_insertion_point(module_scope)
