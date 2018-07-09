#!/usr/bin/python
#
# Copyright (C) 2017 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Test converter of a Config proto.

# Generate with:
#  aprotoc -I=system/extras/perfprofd --python_out=system/extras/perfprofd/scripts \
#      system/extras/perfprofd/binder_interface/perfprofd_config.proto
#
# Note: it is necessary to do a '*' import to not have to jump through hoops
#       with reflective instantiation.
from perfprofd_config_pb2 import *

# Necessary for introspection.
from google.protobuf.descriptor import FieldDescriptor

import sys


def get_type_string(proto_field_type):
    if proto_field_type == FieldDescriptor.TYPE_DOUBLE:
        return "double"
    if proto_field_type == FieldDescriptor.TYPE_FLOAT:
        return "float"
    if proto_field_type == FieldDescriptor.TYPE_INT64:
        return "int64"
    if proto_field_type == FieldDescriptor.TYPE_UINT64:
        return "uint64"
    if proto_field_type == FieldDescriptor.TYPE_INT32:
        return "int32"
    if proto_field_type == FieldDescriptor.TYPE_FIXED64:
        return "fixed64"
    if proto_field_type == FieldDescriptor.TYPE_FIXED32:
        return "fixed32"
    if proto_field_type == FieldDescriptor.TYPE_BOOL:
        return "bool"
    if proto_field_type == FieldDescriptor.TYPE_STRING:
        return "string"
    if proto_field_type == FieldDescriptor.TYPE_GROUP:
        return "group"
    if proto_field_type == FieldDescriptor.TYPE_MESSAGE:
        return "message"
    if proto_field_type == FieldDescriptor.TYPE_BYTES:
        return "bytes"
    if proto_field_type == FieldDescriptor.TYPE_UINT32:
        return "uint32"
    if proto_field_type == FieldDescriptor.TYPE_ENUM:
        return "enum"
    if proto_field_type == FieldDescriptor.TYPE_SFIXED32:
        return "sfixed32"
    if proto_field_type == FieldDescriptor.TYPE_SFIXED64:
        return "sfixed64"
    if proto_field_type == FieldDescriptor.TYPE_SINT32:
        return "sint32"
    if proto_field_type == FieldDescriptor.TYPE_SINT64:
        return "sint64"
    return "unknown type"


def print_selection_index(map, istr):
    maxlen = len(str(sorted(map.iterkeys())[-1]))

    for i in sorted(map.iterkeys()):
        key = str(i)
        while len(key) < maxlen:
            key = ' ' + key
        print('%s%s: %s' % (istr, key, map[i].name))
    print('%s0: done' % (istr))
    print('%s!: end' % (istr))


def read_message(msg_descriptor, indent):
    istr = ' ' * indent
    print('%s%s' % (istr, msg_descriptor.name))
    # Create an instance
    instance = globals()[msg_descriptor.name]()

    # Fill fields.
    # 1) Non-message-type fields. Assume they are not repeated.
    primitive_field_map = {}
    index = 1
    for field in msg_descriptor.fields:
        if field.type != FieldDescriptor.TYPE_MESSAGE:
            primitive_field_map[index] = field
            index += 1

    if len(primitive_field_map) > 0:
        print('%s(Primitives)' % (istr))
        while True:
            print_selection_index(primitive_field_map, istr)
            sel = raw_input('%s ? ' % (istr))
            if sel == '!':
                # Special-case input, end argument collection.
                return (instance, False)

            try:
                sel_int = int(sel)
                if sel_int == 0:
                    break

                if sel_int in primitive_field_map:
                    field = primitive_field_map[sel_int]
                    input = raw_input('%s  -> %s (%s): ' % (istr, field.name,
                                                            get_type_string(field.type)))
                    if input == '':
                        # Skip this field
                        continue
                    if input == '!':
                        # Special-case input, end argument collection.
                        return (instance, False)

                    # Simplification: assume ints or bools or strings, but not floats
                    if field.type == FieldDescriptor.TYPE_BOOL:
                        input = input.lower()
                        set_val = True if input == 'y' or input == 'true' or input == '1' else False
                    elif field.type == FieldDescriptor.TYPE_STRING:
                        set_val = input
                    else:
                        try:
                            set_val = int(input)
                        except:
                            print('Could not parse input as integer!')
                            continue
                    setattr(instance, field.name, set_val)
                else:
                    print('Not a valid input (%d)!' % (sel_int))
                    continue
            except:
                print('Not a valid input! (%s, %s)' % (sel, str(sys.exc_info()[0])))
                continue

    # 2) Message-type fields. These may be repeated.
    message_field_map = {}
    index = 1
    for field in msg_descriptor.fields:
        if field.type == FieldDescriptor.TYPE_MESSAGE:
            message_field_map[index] = field
            index += 1

    if len(message_field_map) > 0:
        print('%s(Nested messages)' % (istr))
        while True:
            print_selection_index(message_field_map, istr)
            sel = raw_input('%s ? ' % (istr))
            if sel == '!':
                # Special-case input, end argument collection.
                return (instance, False)

            try:
                sel_int = int(sel)
                if sel_int == 0:
                    break

                if sel_int in message_field_map:
                    field = message_field_map[sel_int]
                    sub_msg, cont = read_message(field.message_type, indent + 4)
                    if sub_msg is not None:
                        if field.label == FieldDescriptor.LABEL_REPEATED:
                            # Repeated field, use extend.
                            getattr(instance, field.name).extend([sub_msg])
                        else:
                            # Singular field, copy into.
                            getattr(instance, field.name).CopyFrom(sub_msg)
                    if not cont:
                        return (instance, False)
                else:
                    print('Not a valid input (%d)!' % (sel_int))
                    continue
            except:
                print('Not a valid input! (%s, %s)' % (sel, str(sys.exc_info()[0])))
                continue

    return (instance, True)


def collect_and_write(filename):
    config, _ = read_message(ProfilingConfig.DESCRIPTOR, 0)

    if config is not None:
        with open(filename, "wb") as f:
            f.write(config.SerializeToString())


def read_and_print(filename):
    config = ProfilingConfig()

    with open(filename, "rb") as f:
        config.ParseFromString(f.read())

    print config


def print_usage():
    print('Usage: python perf_config_proto.py (read|write) filename')


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print_usage()
    elif sys.argv[1] == 'read':
        read_and_print(sys.argv[2])
    elif sys.argv[1] == 'write':
        collect_and_write(sys.argv[2])
    else:
        print_usage()
