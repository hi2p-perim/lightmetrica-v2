# -*- coding: utf-8 -*-

"""
    cbresolve.py
    Resolve string template while resolving various combination of parameters. 
"""

import os
import argparse
import jinja2
import itertools
import subprocess as sp

def main():
    # Parse comamnd line arguments
    parser = argparse.ArgumentParser()
    parser.add_argument('--infile', '-s', type=str, help='Input scene file')
    parser.add_argument('--entry', '-t', type=str, action='append', help='Add an entry for template dictionary')

    args, unknown = parser.parse_known_args()

    # Create entries
    # Input '-t "key1=[values1]" -t "key2=[values2]" ...'
    # [(key1, list of values1), (key2, list of values2), ...]
    entries = []
    for entry in args.entry:
        key, value = entry.split('=')
        entries.append(
            list((key, v) for v in eval(value))
        )

    # Generate possible combinations 
    for entry in itertools.product(*entries):
        # Create a dict
        context = {}
        for v in entry:
            if type(v) is tuple:
                context[v[0]] = v[1]        

        # Resolve template for scene file name
        filename = jinja2.Template(args.infile).render(context)
        print(filename)

        # Load template
        scene_template_loader = jinja2.FileSystemLoader(os.path.dirname(filename))
        scene_template_env = jinja2.Environment(loader = scene_template_loader)
        scene_template = scene_template_env.get_template(os.path.basename(filename)) 

        # Resolve template
        resolved_scene_template = scene_template.render(context)

        # Resolve template in the additional arguments
        resolved_args = [jinja2.Template(arg).render(context) for arg in unknown]

        # Print message
        print('Started')
        print('Parameters: ' + str(context))
        print('Arguments : ' + str(resolved_args))

        # Dispatch renderer
        p = sp.Popen(
            [
                './lightmetrica',
                'render',
                '-i',
                '-b', os.path.dirname(filename)
            ] + resolved_args,
            stdin=sp.PIPE)
        p.communicate(resolved_scene_template.encode('utf-8'))
        print('Finished')
        
        print('----------------------------------------------------------------------')

if __name__ == '__main__':
    main()
