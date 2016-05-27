# -*- coding: utf-8 -*-

"""
    resolve.py
    Resolve string template using Jinja2.
    Typical usage is to convert templated scene files.
"""

import os
import argparse
import jinja2

def main():
    # Parse comamnd line arguments
    parser = argparse.ArgumentParser()
    parser.add_argument('infile', type=str, help='Input file')
    parser.add_argument('--entry', '-t', type=str, action='append', help='Add an entry for template dictionary')
    args = parser.parse_args()

    # Load template
    template_loader = jinja2.FileSystemLoader(os.path.dirname(args.infile))
    template_env = jinja2.Environment(loader = template_loader)
    template = template_env.get_template(os.path.basename(args.infile))

    # Create dict
    context = {}
    for entry in args.entry:
        key, value = entry.split('=')
        context[key] = value

    # Resolve template
    resolved_template = template.render(context)
    print(resolved_template)

if __name__ == '__main__':
    main()
