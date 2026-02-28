#!/usr/bin/python3
import sys
import os
from typing import Literal
import xml.etree.ElementTree as ET

WL_TYPES = {
    "int":      "c_wl_int",
    "uint":     "c_wl_uint",
    "fixed":    "c_wl_fixed",
    "object":   "c_wl_object_id",
    "new_id":   "c_wl_new_id",
    "string":   "c_wl_string",
    "array":    "c_wl_array",
    "fd":       "c_wl_fd",
    "enum":     "c_wl_enum",
}

SIGNATURE = {
    "int":      "i",
    "uint":     "u",
    "fixed":    "f",
    "object":   "o",
    "new_id":   "n",
    "string":   "s",
    "array":    "a",
    "fd":       "F",
    "enum":     "e",
}


def parse_event(interface_name: str, event: ET.Element, event_n: int, file_type: Literal["h", "c"]) -> str:
    s = """"""
    event_name = event.get("name")

    desc = next(c for c in event if c.tag == "description")
    if file_type == "h" and desc.text:
        s += f" /* {desc.text.strip()} */\n"

    args = [c for c in event if c.tag == "arg"]
    s+=f"C_WL_EVENT {interface_name}_{event_name}(struct c_wl_connection *conn, c_wl_object_id {interface_name}{", " if args else ""}"

    fd = ""
    signature = []
    arg_names = []

    for arg in args:
        attrs = arg.attrib
        arg_wl_type = attrs["type"]
        if not attrs.get("enum"):
            arg_c_type = WL_TYPES[arg_wl_type]
        elif "." in attrs['enum']:
            arg_c_type = f"enum {attrs['enum'].replace('.', '_')}_enum"
        else:
            arg_c_type = f"enum {interface_name}_{attrs['enum']}_enum"
        signature.append(SIGNATURE[arg_wl_type])

        arg_name = attrs["name"] if not attrs.get("interface") else attrs["interface"]
        arg_names.append(arg_name)

        if arg_c_type == "c_wl_fd":
            fd = arg_name

        if arg_c_type == "c_wl_array":
            s+=f"{arg_c_type} *{arg_name}"

        elif arg != args[-1]:
            s+=f"{arg_c_type} {arg_name}, "

        else:
            s+=f"{arg_c_type} {arg_name}"

    signature = f'"{"".join(signature)}"'

    if file_type == "h":
        s+=");\n\n"

    else:
        s+=") {\n"
        s+=f"  struct c_wl_message msg = {{{interface_name}, {event_n}, {signature if args else "{0}"}, \"{event_name}\"}};\n"
        s+=f"  return c_wl_connection_send(conn, &msg, {len(args)}{", " + ', '.join(arg_names) if args else ""});\n"
        s+="}\n"

    return s

def parse_events(interface_name: str, events: list[ET.Element], file_type: Literal["h", "c"]) -> str:
    s = """"""
    for i, ev in enumerate(events):
        s+=parse_event(interface_name, ev, i, file_type)
    return s

def parse_request(interface_name: str, request: ET.Element, file_type: Literal["h", "c"]) -> tuple[str, str]:
    struct = """"""
    decl = """"""

    request_name = request.get("name")

    desc = next(c for c in request if c.tag == "description")
    if interface_name == "wl_registry" and request_name == "bind":
        args = [
            ET.Element("arg", {"name": "name",      "type": "uint"}),
            ET.Element("arg", {"name": "interface", "type": "string"}),
            ET.Element("arg", {"name": "version",   "type": "uint"}),
            ET.Element("arg", {"name": "id",        "type": "new_id"}),
        ]
    else:
        args = [c for c in request if c.tag == "arg"]

    nargs = len(args)

    signature_list = [SIGNATURE[arg.attrib['type']] for arg in args]
    signature = f'"{"".join(signature_list)}"'

    struct +=  f'    {{{f"\"{request_name}\",":<25} {f"{interface_name}_{request_name},":<30}NULL, {f"{nargs},":<3} {signature if nargs > 0 else '{0}'}}},\n'

    if file_type == "h":
        if desc.text:
            if args:
                decl += f"   /* {desc.text.strip()}\n\n"

            else: 
                decl += f"   /* {desc.text.strip()} */\n"

        elif args:
            decl = "   /*\n"


        for i, arg in enumerate(args, 1):
            attrs = arg.attrib
            arg_name = attrs["name"] if not attrs.get("interface") else attrs["interface"]
            arg_wl_type = attrs["type"]
            if not attrs.get("enum"):
                arg_c_type = WL_TYPES[arg_wl_type]
            elif "." in attrs['enum']:
                arg_c_type = f"enum {attrs['enum'].replace('.', '_')}_enum"
            else:
                arg_c_type = f"enum {interface_name}_{attrs['enum']}_enum"

            decl+=f"    @arg{i}: {arg_c_type} {arg_name}\n"

        if args:
            decl += "   */\n"

    decl+=f"C_WL_REQUEST {interface_name}_{request_name}(struct c_wl_connection *conn, union c_wl_arg *args, void *userdata);\n"

    return decl, struct

def parse_requests(interface: ET.Element, requests: list[ET.Element], file_type: Literal["h", "c"]) -> str:
    s = """"""
    if not requests:
        return ""

    interface_name = interface.get('name', '')
    interface_version = interface.get('version', 1)
    ss = [parse_request(interface_name, req, file_type) for req in requests]

    for decl, _ in ss:
        s += decl + "\n"

    if file_type == "c":
        s+=f"C_WL_INTERFACE_REGISTER({interface_name}_interface, \"{interface_name}\", {interface_version}, {len(ss)}, \n"
        for _, struct in ss:
            s+=struct
        s+=")\n"

    return s

def parse_enum(interface_name: str, enum: ET.Element) -> str:
    s = """"""

    desc = next((c for c in enum if c.tag == "description"), None)
    if desc is not None and desc.text:
        s += f" /* {desc.text.strip()} */\n"

    basenane = f"{interface_name}_{enum.get('name')}"
    s += f"enum {basenane}_enum {{\n"
    for child in enum:
        if child.tag == "entry":
            entry_name = child.get('name')
            assert(entry_name)
            s += f"  {basenane.upper()}_{entry_name.upper()} = {child.get('value')},\n"

    s += "};\n"
    return s

def parse_enums(interface_name: str, enums: list[ET.Element]) -> str:
    s = """"""
    for enum in enums:
        s+=parse_enum(interface_name, enum)+'\n'
    return s


def parse_interface(interface: ET.Element, file_type: Literal["h", "c"]) -> tuple[list[str], str]:
    s = """"""
    attrs = interface.attrib
    interface_name = attrs['name']
    events =[]
    requests = []
    enums = []
    enums_s = []

    for child in interface:
        match child.tag:
            case "event": events.append(child)
            case "request": requests.append(child) 
            case "enum": enums.append(child)

    enums_s.append(parse_enums(interface_name, enums))
    s+=parse_events(interface_name, events, file_type)
    s+=parse_requests(interface, requests, file_type)

    s+="\n"
    return enums_s, s

def parse(xml_path: str, basename: str) -> None:
    tree = ET.parse(xml_path)
    root = tree.getroot()

    h_guard_name = f"CUTS_{os.path.basename(basename).replace("-", "_").upper()}_H"

    with open(f"{basename}.h", "w") as f:
        f.write(f'#ifndef {h_guard_name}\n')
        f.write(f'#define {h_guard_name}\n\n')

        f.write('#include "wayland/server.h"\n')
        f.write('#include <stdint.h>\n\n\n')

        enums_s = []
        interfaces_s = []

        for child in root:
            if child.tag == "interface":
                enums, interface = parse_interface(child, "h")
                enums_s.append(enums)
                interfaces_s.append(interface)

        for enum in [b for a in enums_s for b in a]:
            f.write(enum)

        for iface in interfaces_s:
            f.write(iface)

        f.write("#endif")

    with open(f"{basename}.c", "w") as f:
        f.write('#include "wayland/server.h"\n')
        f.write(f'#include "wayland/types/{os.path.basename(basename)}.h"\n\n')
        f.write('#include <stdint.h>\n\n\n')

        for child in root:
            if child.tag == "interface":
                f.write(parse_interface(child, "c")[1])

def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <path-to-protocol> <output-basename>")
        return

    xml_path = sys.argv[1]
    basename = sys.argv[2]

    parse(xml_path, basename)


main()
