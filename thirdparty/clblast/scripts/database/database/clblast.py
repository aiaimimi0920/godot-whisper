
# This file is part of the CLBlast project. The project is licensed under Apache Version 2.0. This file follows the
# PEP8 Python style guide and uses a max-width of 120 characters per line.
#
# Author(s):
#   Cedric Nugteren <www.cedricnugteren.nl>

import os

# Type settings (also change in database_structure.hpp)
STRING_LENGTH = 50
PARAMETERS_LENGTH = 16

# Constants from the C++ code
VENDOR_DEFAULT = "default"
DEVICE_TYPE_DEFAULT = "All"
DEVICE_NAME_DEFAULT = "default"
DEVICE_NAME_DEFAULT_CONSTANT = "kDeviceNameDefault                                        "
DEVICE_ARCHITECTURE_DEFAULT = "default"

# List of attributes
DEVICE_TYPE_ATTRIBUTES = ["clblast_device_vendor", "clblast_device_type"]
DEVICE_ATTRIBUTES = ["clblast_device_name", "clblast_device_architecture",
                     "device_core_clock", "device_compute_units"]
KERNEL_ATTRIBUTES = ["precision", "kernel_family"]
ARGUMENT_ATTRIBUTES = ["arg_m", "arg_n", "arg_k", "arg_alpha", "arg_beta",
                       "arg_from", "arg_to", "arg_step",
                       "arg_channels", "arg_height", "arg_width", "arg_kernel_h", "arg_kernel_w",
                       "arg_num_kernels", "arg_batch_count"]
ATTRIBUTES = DEVICE_ATTRIBUTES + DEVICE_TYPE_ATTRIBUTES + KERNEL_ATTRIBUTES + ARGUMENT_ATTRIBUTES
GROUP_ATTRIBUTES = DEVICE_TYPE_ATTRIBUTES + KERNEL_ATTRIBUTES + ["kernel"] + ARGUMENT_ATTRIBUTES

# Other constants
VENDORS_WITH_ARCHITECTURE = ["AMD", "NVIDIA"]

def precision_to_string(precision):
    """Translates a precision number (represented as Python string) into a descriptive string"""
    if precision == "16":
        return "Half"
    elif precision == "32":
        return "Single"
    elif precision == "64":
        return "Double"
    elif precision == "3232":
        return "ComplexSingle"
    elif precision == "6464":
        return "ComplexDouble"
    else:
        raise("Unknown precision: " + precision)


def get_cpp_separator():
    """Retrieves a C++ comment separator"""
    return "// ================================================================================================="


def get_cpp_header(family, precision):
    """Retrieves the C++ header"""
    return ("\n" + get_cpp_separator() + """
// This file is part of the CLBlast project. The project is licensed under Apache Version 2.0. It
// is auto-generated by the 'scripts/database/database.py' Python script.
//
// This file populates the database with best-found tuning parameters for the '%s%s' kernels.
//\n"""
            % (family.title(), precision)) + get_cpp_separator() + "\n"


def get_cpp_header_namespace():
    return "\nnamespace clblast {\n" + "namespace database {\n"


def get_cpp_footer():
    """Retrieves the C++ footer"""
    return "\n} // namespace database\n" + "} // namespace clblast\n"


def get_cpp_precision(family, precision):
    """Retrieves the C++ code for the start of a new precision"""
    precision_string = precision_to_string(precision)
    camelcase_name = family.title().replace("_", "")
    return("\nconst DatabaseEntry %s%s = {\n  \"%s\", Precision::k%s"
           % (camelcase_name, precision_string, camelcase_name, precision_string))


def get_cpp_device_vendor(vendor, device_type):
    """Retrieves the C++ code for the (default) vendor and device type"""
    if vendor == VENDOR_DEFAULT and device_type == DEVICE_TYPE_DEFAULT:
        return "    { // Default\n      kDeviceType%s, \"%s\", {\n" % (device_type, vendor)
    device_type_caps = device_type[0].upper() + device_type[1:]
    return "    { // %s %ss\n      kDeviceType%s, \"%s\", {\n" % (vendor, device_type, device_type_caps, vendor)


def get_cpp_family_includes(family, precisions):
    result = "\n"
    result += "#include \"database/kernels/%s/%s.hpp\"\n" % (family, family)
    for precision in precisions:
        result += "#include \"database/kernels/%s/%s_%s.hpp\"\n" % (family, family, precision)
    return result


def get_hpp_family_includes(family, precisions):
    result = "\n"
    result += "#include \"database/database_structure.hpp\"\n"
    result += "\n"
    result += "namespace clblast {\n"
    result += "namespace database {\n"
    result += "\n"
    camelcase_name = family.title().replace("_", "")
    for precision in precisions:
        precision_string = precision_to_string(precision)
        result += "extern const DatabaseEntry %s%s;\n" % (camelcase_name, precision_string)
    result += "\n"
    result += "} // namespace database\n"
    result += "} // namespace clblast\n"
    return result


def print_as_name(name):
    return "Name{\"%-50s\"}" % name.strip()[:STRING_LENGTH]


def get_kernel_database_results(kernel_database):
    """Retrieves the best result from a group of results. Asserts for valid data"""
    assert len(kernel_database) >= 1

    all_results = [item["results"] for item in kernel_database]

    best_results = all_results[0]
    for results in all_results:

        # Debugging in case of unexpected results
        length_assumption = (len(results) == 1)
        params_assumption = (sorted(results[0]["parameters"]) == sorted(best_results[0]["parameters"]))
        if not length_assumption or not params_assumption:
            print("[database] ERROR: Found %d kernel databases, expected 1" % len(kernel_database))
            all_keys = sorted([key for item in kernel_database for key in item.keys()])
            missing_keys = set([x for x in all_keys if all_keys.count(x) != len(kernel_database)])
            print("[database] All keys in databases: %s" % str(set(all_keys)))
            print("[database] Missing keys in one or more databases: %s" % str(missing_keys))
            for index, item in enumerate(kernel_database):
                print("[database] %d:" % index)
                print(item)
        assert length_assumption
        assert params_assumption

        if results[0]["time"] < best_results[0]["time"]:
            best_results = results

    return best_results


def print_cpp_database(database, output_dir):
    """Outputs the database as C++ code"""

    # Iterates over the kernel families
    kernel_families = sorted(set([s["kernel_family"] for s in database["sections"]]))
    for family_name in kernel_families:
        family_database = [s for s in database["sections"] if s["kernel_family"] == family_name]

        # Goes into a new path for each kernel family
        family_path = os.path.join(output_dir, family_name)

        # Loops over the different precision (e.g. 16, 32, 3232, 64, 6464)
        precisions = sorted(set([s["precision"] for s in database["sections"]]))  # Based on full database
        for precision in precisions:
            precision_database = [s for s in family_database if s["precision"] == precision]

            # Opens a new file for each precision
            full_path = os.path.join(family_path, family_name + "_" + precision + ".hpp")
            with open(full_path, 'w+') as f:
                f.write(get_cpp_header(family_name, precision))
                f.write(get_cpp_header_namespace())
                f.write(get_cpp_precision(family_name, precision))

                # In case there is nothing found at all (e.g. 16-bit): continue as if this was a
                #  precision of 32 but with the defaults only
                if len(precision_database) == 0:
                    print("[database] No results found for %s:%s, retrieving defaults from %s:32" %
                          (family_name, precision, family_name))
                    precision_database = [s for s in family_database if s["precision"] == "32"
                                          and s["clblast_device_vendor"] == VENDOR_DEFAULT
                                          and s["clblast_device_type"] == DEVICE_TYPE_DEFAULT
                                          and s["clblast_device_name"] == DEVICE_NAME_DEFAULT]

                # Discovers the parameters for this kernel
                parameter_names = []
                for example_data in precision_database:
                    for example_result in example_data["results"]:
                        parameter_names.extend([str(k) for k in example_result["parameters"].keys()])
                parameter_names = sorted(list(set(parameter_names)))
                parameter_names_as_string = ", ".join(['"%s"' % p for p in parameter_names])
                f.write(", {" + parameter_names_as_string + "}, {\n")

                # Loops over device vendors (e.g. AMD)
                device_vendors = sorted(set([s["clblast_device_vendor"] for s in precision_database]))
                for vendor in device_vendors:
                    vendor_database = [s for s in precision_database if s["clblast_device_vendor"] == vendor]

                    # Loops over device types (e.g. GPU)
                    device_types = sorted(set([s["clblast_device_type"] for s in vendor_database]))
                    for device_type in device_types:
                        type_database = [s for s in vendor_database if s["clblast_device_type"] == device_type]
                        f.write(get_cpp_device_vendor(vendor, device_type))

                        # Loops over every architecture of this vendor-type combination
                        architectures = sorted(set([s["clblast_device_architecture"] for s in type_database]))
                        if vendor in VENDORS_WITH_ARCHITECTURE:
                            architectures = [a for a in architectures if a != ""]
                        for architecture in architectures:
                            architecture_database = [s for s in type_database if s["clblast_device_architecture"] == architecture]
                            architecture_string = DEVICE_ARCHITECTURE_DEFAULT if architecture == "" else architecture
                            f.write("        { \"%s\", {\n" % architecture_string)

                            # Loops over every device of this vendor-type combination
                            devices = sorted(set([s["clblast_device_name"] for s in architecture_database]))
                            for device_name in devices:
                                device_database = [s for s in architecture_database if s["clblast_device_name"] == device_name]
                                device_name_as_string = print_as_name(device_name) if device_name != DEVICE_NAME_DEFAULT else DEVICE_NAME_DEFAULT_CONSTANT
                                device_name_cpp = "          { %s, Params{ " % device_name_as_string
                                f.write(device_name_cpp)

                                # Collects the parameters for this entry
                                parameters = []
                                parameter_index = 0
                                kernels = sorted(set([s["kernel"] for s in device_database]))
                                for kernel in kernels:
                                    kernel_database = [s for s in device_database if s["kernel"] == kernel]
                                    results = get_kernel_database_results(kernel_database)

                                    assert len(results) == 1
                                    new_parameters = results[0]["parameters"]
                                    for parameter_name in sorted(new_parameters):
                                        assert parameter_name == parameter_names[parameter_index]
                                        parameter_value = new_parameters[parameter_name]
                                        parameters.append(str(parameter_value))
                                        parameter_index += 1

                                # Appends zero's to complete the list
                                assert parameter_index <= PARAMETERS_LENGTH
                                for append_index in range(parameter_index, PARAMETERS_LENGTH):
                                    parameters.append("0")

                                # Prints the entry
                                f.write(", ".join(parameters))
                                f.write(" } },\n")

                            # Prints the architecture footer
                            f.write("        } },\n")

                        # Prints the vendor-type combination footer
                        f.write("      }\n    },\n")

                # Prints the precision footer
                f.write("  }\n};\n")

                # Prints the file footer
                f.write(get_cpp_footer())

            # Creates the combined family sources
            full_path = os.path.join(family_path, family_name + ".cpp")
            with open(full_path, 'w+') as f:
                f.write(get_cpp_header(family_name, ""))
                f.write(get_cpp_family_includes(family_name, precisions))

            # Creates the combined family includes header
            full_path = os.path.join(family_path, family_name + ".hpp")
            with open(full_path, 'w+') as f:
                f.write(get_cpp_header(family_name, ""))
                f.write(get_hpp_family_includes(family_name, precisions))