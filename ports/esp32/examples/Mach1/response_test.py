import multipython as mp
import time
import addressable
import mach1

# get the context of this process
context = mp.context()

# make up some arguments to identify responses (these only need to be unique within this file/context)
resume = 'resume'
suspend = 'suspend'

# register responses to events
mp.response(condition = mach1.FALLING_USW1, context = context, control_op = mp.CONTROL_RESUME, argument = resume)
mp.response(condition = mach1.RISING_USW1, context = context, argument = suspend)

# Get the status fixture and add a layer
stat = addressable.controller(addressable.STAT_CONTROLLER)
statFix = stat.fixtures(0)
taskLayer = statFix.add_layer() 
taskLayer.mode(taskLayer.SKIP)		# set the layer mode to SKIP to hide the LED at first
taskLayer.set(0,[[0x80,0,0,0]])		# use the composite mode to add red to the status LED

# # After setup is all complete we want to suspend the task until it is triggered by the USW1_FALLING condition
mp.control(ids = None, op = mp.CONTROL_SUSPEND)			# use 'None' as id argument to affect this task

while(1):
	time.sleep_ms(60)	# this delay allows other tasks to get some processing time too
	# print("Hello from the test task") # commented b/c status LED will now be the indicator

	# Use 'check_responses()' to get the argument of the response that matched the next condition
	argument = mp.check_responses()

	# # Now we can handle the arguments as needed
	# if(argument == None):
	# 	print('Arg was None')							# commented b/c this tends to clog up the command line
	
	if(argument == resume):
		taskLayer.mode(taskLayer.COMP)					# change to composite (COMP) mode when resuming
		print('Arg was ' + str(resume))

	if(argument == suspend):
		print('Arg was ' + str(suspend))
		print('About to suspend the task')
		taskLayer.mode(taskLayer.SKIP)					# when turning the task off we want to skip the layer in the output computation
		mp.control(ids = None, op = mp.CONTROL_SUSPEND)




