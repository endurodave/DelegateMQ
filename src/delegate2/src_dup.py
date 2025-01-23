# Script to keep common sections of the repetitive code and data 
# synchronized within the same file. The code between the first 
# pare of a tags within a source code file is automatically duplicated 
# to all other tag locations. For instance, change MyClass1::Func() and 
# classes below will be updated to match.
#
#
# class MyClass1 {
# public:
# // <common>
# void Func() {}
# // </common>
# }
#
# class MyClass2 {
# public:
# // <common>
# void Func() {}
# // </common>
# }
#
# Tags: 
# <common_code> </common_code>
#
# Run:
# python src_dup.py <file_name>
#
# python src_dup.py DelegateAsync.h
# python src_dup.py DelegateAsyncWait.h

import re
import sys

def copy_common_code(input_code, tag):
    # Regular expression to match content within tag
    common_pattern = re.compile(tag, re.DOTALL)
    
    # Find all the content inside the tag
    common_code_match = re.search(common_pattern, input_code)
    
    if common_code_match:
        common_code = common_code_match.group(0)
        
        # Replace the tags content 
        output_code = re.sub(common_pattern, f"{common_code}", input_code)
        
        return output_code
    else:
        return input_code

def process_file(file_name):
    try:
        # Open the file and read the content
        with open(file_name, 'r') as file:
            input_code = file.read()

        # Process the code and get the modified version
        modified_code = copy_common_code(input_code, r'// <common_code>(.*?)// </common_code>')

        # Output the modified code 
        with open(file_name, 'w') as output_file:
            output_file.write(modified_code)
        
        print(f"Modified code has been written to {file_name}")
    
    except FileNotFoundError:
        print(f"Error: The file '{file_name}' was not found.")
    except ValueError as e:
        print(e)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python script.py <input_file_name>")
    else:
        input_file_name = sys.argv[1]
        process_file(input_file_name)
