import os
import platform;
if platform.system() == 'Windows':
	env = Environment(ENV = os.environ,tools=[])
	env.Tool('mingw')
else:
	env = Environment(ENV = os.environ)
	if ARGUMENTS.get('crossmingw', '0') == '1':
		env.Tool('crossmingw', toolpath=['.'])
env['LIBS'] = 'z'
env.Program('sob', Glob('*.c'), LINKFLAGS='-static')

