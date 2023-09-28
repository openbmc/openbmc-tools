import re
import subprocess
import json
import copy
import sys
from collections import OrderedDict
def printContent(content):
	'''Prints the information regarding each target'''
	for key in content.keys():
		print(key+" = "+content[key])
def assignValues(content):
	'''Takes each target list as input and converts it into a dictionary of key_value pairs '''
	key_val=OrderedDict()
	key_val['TARGET']=content[0].strip(" = ")
	hwas=['','','FUNCTIONALOVERRIDE','SPECDECONFIG','DUMPFUNCTIONAL','FUNCTIONAL','PRESENT','POWEREDON']
	for i in range(1,len(content)):
		data_type=content[i][0]+'_DATATYPE'
		content[i][2]=content[i][2].strip("\"")#removes double quotes(") if any from the value part of each attribute
		key_val[content[i][0]]=" ".join(content[i][2:])
		if(content[i][0]=='ATTR_HWAS_STATE'):
			key_val['ATTR_HWAS_STATE_COMPLETE']=key_val[content[i][0]]
			key_val['ATTR_HWAS_STATE']=content[i][-1]
			state=content[i][-1]
			state=int(state,16)#state is a hexadecimal string, converts state into an integer
			for j in range(2,8):#bits 0,1(7,8 from lsb) are reserved, so checking if other bits are set
				is_set=state>>j#right shift j bits so that jth  bit will become lsb
				if(is_set & 1):#Performing logical & with 1 so that if the lsb is set then the value is true
					key_val[content[i][0]+"."+hwas[j]]="True"
				else:
					key_val[content[i][0]+"."+hwas[j]]="False"
		key_val[data_type]=content[i][1]
	return key_val
def openFile(name):
	'''This function takes a file as input,opens it and returns a list of targets'''
	with open(name,'r') as file:#open file in read mode
		lines=file.read()#read the file
		lines=lines.split("target")#split the file into a list of targets
	return lines#returns a list where each element is a string representing the whole target info
def splitValues(content):
	'''This function takes each target attribute and creates a list consisting of attribute_name, type and value.'''
	i=1
	flag=0
	while(i<len(content)):
		content[i]=content[i].split()
		if("ATTR_PHYS_BIN_PATH" in content[i][0]):
			if(flag==1):
				content[i-1].append(content[i][-1])
				del content[i]
				i-=1
			else:
				content[i][0]="ATTR_PHYS_BIN_PATH"
				flag=1
		i+=1
	return content 
def getPath(content):
	'''This function takes a target and returns the Physical path'''
	#print(content)
	path=content['ATTR_PHYS_DEV_PATH']
	path=path.strip("\"")
	return path[9:]
def listNodes(content,flag=[]):
	'''This function returns the hierarchy in which nodes appear in the device tree'''
	path=getPath(content)
	if(flag!=[]):
		for i in flag:
			if(i not in path):
				return
	path=path.split("/")
	node_dict=Nodes
	for i in path:
		if(i not in node_dict.keys()):
			node_dict[i]={}
			node_dict=node_dict[i]
		else:
			node_dict=node_dict[i]
def sortNodes(Nodes):
	'''This function sorts the nodes in logical ascending order'''
	Nodes=OrderedDict(sorted(Nodes.items(), key=lambda x: (x[0][0:x[0].rfind('-')],int(x[0][x[0].rfind('-')+1:]))))
	for i in Nodes.keys():
		if(Nodes[i]!={}):
			Nodes[i]=sortNodes(Nodes[i])
	return Nodes
def printNodes(node,space=''):
        '''This function prints the nodes in hierarchy '''
        global t,l
        if(node is None):
                return
        for i in node.keys():
                l.append(i)
                print(space+t+("/".join(l)))
                t="|_"
                if(node[i]!={}):
                        printNodes(node[i],space+'  ')
                del l[-1]
                if(space==''):
                    t=''
def printSubTree(node,path):
    '''Prints the subtree of the path specified '''
    global t,l
    path=path.split('/')
    node_dict=OrderedDict(copy.deepcopy(node))
    for i in range(len(path)):
        l.append(path[i])
        node_dict=node_dict[path[i]]
    print(t+("/".join(l)))
    t="|_"
    printNodes(node_dict,space=' ')
def processFile(file_content):
	'''This function takes the list returned by openFile() as input and returns a dictionary '''
	for i in range(len(file_content)):
		file_content[i]=file_content[i].split("\n")
		del file_content[i][-1]#last element will be [], so removing it
		if(file_content[i]==[]):
			continue
		file_content[i]=splitValues(file_content[i])#Attributes are of the form <name> <type> <val>, they are converted into a list [name,type,val]
		file_content[i]=assignValues(file_content[i])#returns a dictionary of attributes
		if('core-0' in file_content[i]['ATTR_PHYS_DEV_PATH']):
			file_content[i-1]['ATTR_ECO_MODE']=file_content[i]['ATTR_ECO_MODE']
			file_content[i-1]['ATTR_ECO_MODE_DATATYPE']=file_content[i]['ATTR_ECO_MODE_DATATYPE']
	return file_content#returns a list of dictionaries
def ListTargets(tar_list):
	for i in tar_list:
		print(i)
def getLimitedAttributes(content,attrs):
	'''Takes the dictionary of attributes and returns a dictionary of specified attributes '''
	attrs=[i.upper() for i in attrs]
	cont=OrderedDict()
	for i in content.keys():
		for j in attrs:
			if(j in i):
				cont[i]=content[i]
	return cont
def printSpecific(content,fc):
	global flag,json_file
	if(len(sys.argv)==3 and fc==sys.argv[2]):
		flag=1
		printContent(content)
		print("\n")
	elif(len(sys.argv)>=4 and fc==sys.argv[2]):
		if(sys.argv[3]=='-j' or sys.argv[3]=='-json'):
			if(len(sys.argv)==4):
				flag=1
				js=json.dumps(content,indent=4)
				json_file.write(js)
				print(js)
			else:
				if(sys.argv[4]=='-limited'):
					attrs=['ATTR_ECO_MODE','ATTR_HWAS_STATE','TARGET','ATTR_PHYS_DEV_PATH']
				else:
					attrs=sys.argv[4].split(",")
				cont=getLimitedAttributes(content,attrs)
				flag=1
				js=json.dumps(cont,indent=4)
				json_file.write(js)
				print(js)
		else:
			if(sys.argv[3]=='-limited'):
				attrs=['ATTR_HWAS_STATE','TARGET',"ATTR_PHYS_DEV_PATH",'ATTR_ECO_MODE']
			else:
				attrs=sys.argv[3].split(",")
			cont=getLimitedAttributes(content,attrs)
			flag=1
			printContent(cont)
			print("\n")
def printTargets(file_content,option):
	'''Based on option specified, necessary info is printed '''
	global flag
	global Nodes,json_file
	k_v=OrderedDict()
	tar_list=[]
	for i in range(len(file_content)):
		if(len(file_content[i]) and option==0):
			if(len(sys.argv)==1):
				tar='target : '+file_content[i]['TARGET']
				print(tar.center(50,'*'))
				printContent(file_content[i])#prints whole content in normal text format
				print("\n")
			else:
				if(sys.argv[1]=='-j'):
					if(len(sys.argv)==2):
						key=file_content[i]['ATTR_PHYS_DEV_PATH'].split(":")
						key=key[-1].split("/")
						k=k_v
						j=0
						while(j<len(key)-1):
							if(key[j] in k.keys()):
								k=k[key[j]]
							else:
								k[key[j]]=OrderedDict()
								k=k[key[j]]
							j+=1
						k[key[-1]]=file_content[i]
					else:
						if(sys.argv[2]=='-limited'):
							attrs=['TARGET','ATTR_HWAS_STATE','ATTR_PHYS_DEV_PATH','ATTR_ECO_MODE']
						else:
							attrs=sys.argv[2].split(",")
						cont=getLimitedAttributes(file_content[i],attrs)#gets a list of attributes specified by the user
						key=file_content[i]['ATTR_PHYS_DEV_PATH'].split(":")
						key=key[-1].split("/")
						k=k_v
						j=0
						while(j<len(key)-1):
							#print(k.keys())
							if(key[j] in k.keys()):
								k=k[key[j]]
							else:
								k[key[j]]=OrderedDict()
								k=k[key[j]]
							j+=1
						k[key[-1]]=cont
				else:
					if(sys.argv[1]=='-limited'):
						attrs=['TARGET','ATTR_HWAS_STATE','ATTR_PHYS_DEV_PATH','ATTR_ECO_MODE']
					else:
						attrs=sys.argv[1].split(",")
					cont=getLimitedAttributes(file_content[i],attrs)#gets a list of attributes specified by user
					print()
					tar='target : '+file_content[i]['TARGET']
					print(tar.center(50,'*'))
					printContent(cont)#prints only the specified attributes in normal text format
		elif(len(file_content[i]) and option=='-l'):
			tar_list.append(file_content[i]['TARGET'])
		elif(len(file_content[i]) and option=='-t'):
			fc=file_content[i]['TARGET']
			printSpecific(file_content[i],fc)
			if(flag==1):
				break
		elif(len(file_content[i]) and option=='-p'):
			path=getPath(file_content[i])
			printSpecific(file_content[i],path)
			if(flag==1):
				break
		elif(len(file_content[i]) and option=='-n'):
			flag=1
			if(len(sys.argv)>2 and sys.argv[2]!='-fullpath'):
				listNodes(file_content[i],sys.argv[2].split("/"))
			else:
				listNodes(file_content[i])
		elif(len(file_content[i]) and option=='-f'):
			attrs=['TARGET','ATTR_HWAS_STATE','ATTR_PHYS_DEV_PATH','ATTR_ECO_MODE']
			if(len(sys.argv)==3):
				if(re.search("^"+sys.argv[2],file_content[i]['ATTR_PHYS_DEV_PATH'].split("/")[-1])):
					printContent(file_content[i])
					print()
			elif(len(sys.argv)>3 and sys.argv[3]=='-limited'):
				if(re.search("^"+sys.argv[2],file_content[i]['ATTR_PHYS_DEV_PATH'].split("/")[-1])):
					cont=getLimitedAttributes(file_content[i],attrs)
					printContent(cont)
					print()
			elif(len(sys.argv)>3):
				flag=0
				if(re.search("^"+sys.argv[2],file_content[i]['ATTR_PHYS_DEV_PATH'].split("/")[-1])):
					for x in sys.argv[3:]:
						x=x.split("=")
						if(file_content[i][x[0].upper()].upper()!=x[1].upper()):
							flag=1
					if(flag==0):
						cont=getLimitedAttributes(file_content[i],attrs)
						printContent(cont)
						print()
	if(option=='-n'):
		l=[]
		Nodes=sortNodes(Nodes)
		if(len(sys.argv)==2):
			printNodes(Nodes)
		else:
			if(sys.argv[2]=='-fullpath'):
				printSubTree(Nodes,sys.argv[3])
			else:
				printNodes(Nodes)	
	elif(option=='-l'):
		ListTargets(tar_list)
	elif(option==0 and k_v!={}):
		js=json.dumps(k_v,indent=4)
		json_file.write(js)
		print(js)
def invalidDetails(arg):
	'''This function tells the user to enter valid details. '''
	if(arg=='-t'):
                print("please enter a valid target")
	elif(arg=='-p'):
		print("please enter a valid path")
def getChild(file_content,target):
	path_list=OrderedDict()
	if('bmc' not in host_name.stdout.decode('utf-8')):
			for i in range(len(file_content)):
				if(file_content[i]==[]):
					continue
				tar=file_content[i]['TARGET']
				if(tar==target):
					ssh_client=paramiko.SSHClient()
					ssh_client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
					ssh_client.connect(host,port=22, username=uname, password=pwd)
					if('dimm' in target):
						j=i+1
						while(j<len(file_content) and ('pmic' in file_content[j]['TARGET'] or 'adc' in file_content[j]['TARGET'])):
							cmd='sudo -i attributes translate'+" "+file_content[j]['TARGET']
							stdin, stdout, stderr = ssh_client.exec_command(cmd)
							x=stdout.read().decode('utf-8').strip("\n")
							x=x.split(" ")
							if('/' not in x[0]):
								x[0],x[-1]=x[-1],x[0]
							path_list[x[-1]]=x[0]
							j+=1
						print(json.dumps(path_list,indent=4))
						ssh_client.close()
						exit()
					else:
						j=i
						cmd='sudo -i attributes translate'+" "+file_content[j]['TARGET']
						stdin, stdout, stderr = ssh_client.exec_command(cmd)
						res=stdout.read().decode('utf-8').strip("\n")
						tt=res[res.rfind("/")+1:]
						while(j<len(file_content) and tt in res):
							j+=1
							cmd='sudo -i attributes translate'+" "+file_content[j]['TARGET']
							stdin, stdout, stderr = ssh_client.exec_command(cmd)
							res=stdout.read().decode('utf-8').strip("\n")
							if(tt not in res):
								break
							x=res.split(" ")
							if('/' not in x[0]):
								x[0],x[-1]=x[-1],x[0]
							path_list[x[-1]]=x[0]
						print(json.dumps(path_list,indent=4))
						ssh_client.close()
						exit()
	else:
		for i in range(len(file_content)):
				if(file_content[i]==[]):
					continue
				tar=file_content[i]['TARGET']
				if(tar==target):
					if('dimm' in target):
						j=i+1
						while(j<len(file_content) and ('pmic' in file_content[j]['TARGET'] or 'adc' in file_content[j]['TARGET'])):
							cmd='attributes translate'+" "+file_content[j]['TARGET']
							res=subprocess.Popen(cmd,shell=True,stdout=subprocess.PIPE)
							output,_=res.communicate()
							out=output.decode("utf8")
							x=out.strip("\n")
							x=x.split(" ")
							if('/' not in x[0]):
								x[0],x[-1]=x[-1],x[0]
							path_list[x[-1]]=x[0]
							j+=1
						print(json.dumps(path_list,indent=4))
						exit()
					else:
						j=i
						cmd='attributes translate'+" "+file_content[j]['TARGET']
						res=subprocess.Popen(cmd,shell=True,stdout=subprocess.PIPE)
						output,_=res.communicate()
						out=output.decode("utf8")
						tt=out[out.rfind("/")+1:]
						while(j<len(file_content) and (tt in out)):
							j+=1
							cmd='attributes translate'+" "+file_content[j]['TARGET']
							res=subprocess.Popen(cmd,shell=True,stdout=subprocess.PIPE)
							output,_=res.communicate()
							out=output.decode("utf8")
							if(tt not in out):
								break
							x=out.strip("\n").split(" ")
							if('/' not in x[0]):
								x[0],x[-1]=x[-1],x[0]
							path_list[x[-1]]=x[0]
						print(json.dumps(path_list,indent=4))
						exit()
def getParent(file_content,target):
	path_list=OrderedDict()
	if('bmc' not in host_name.stdout.decode('utf-8')):
		if('pmic' in target or 'adc' in target):
			for i in range(len(file_content)):
				if(file_content[i]==[]):
					continue
				tar=file_content[i]['TARGET']
				if(tar==target):
					j=i-1
					while(j>=0 and 'dimm' not in file_content[j]['TARGET']):
						j-=1
					ssh_client=paramiko.SSHClient()
					ssh_client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
					ssh_client.connect(host,port=22, username=uname, password=pwd)
					cmd='sudo -i attributes translate'+" "+file_content[j]['TARGET']
					stdin, stdout, stderr = ssh_client.exec_command(cmd)
					x=stdout.read().decode('utf-8').strip("\n")
					x=x.split(" ")
					if('/' not in x[0]):
						x[0],x[-1]=x[-1],x[0]
					path_list[x[-1]]=x[0]
					print(json.dumps(path_list,indent=4))
					ssh_client.close()
					exit()
		else:
			ssh_client=paramiko.SSHClient()
			ssh_client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
			ssh_client.connect(host,port=22, username=uname, password=pwd)
			cmd='sudo -i attributes translate'+" "+target
			stdin, stdout, stderr = ssh_client.exec_command(cmd)
			res=stdout.read().decode('utf-8')
			p=res.split(" ")[-1]
			x=p.rfind('/')
			p=p[:x]
			cmd='sudo -i attributes translate'+" "+p
			stdin, stdout, stderr = ssh_client.exec_command(cmd)
			res=stdout.read().decode('utf-8')
			while('Failure' in res):
				x=p.rfind('/')
				p=p[:x]
				cmd='sudo -i attributes translate'+" "+p
				stdin, stdout, stderr = ssh_client.exec_command(cmd)
				res=stdout.read().decode('utf-8')
			x=res.strip("\n")
			x=x.split(" ")
			if('/' not in x[0]):
				x[0],x[-1]=x[-1],x[0]
			path_list[x[-1]]=x[0]
			print(json.dumps(path_list,indent=4))
			ssh_client.close()
			exit()
	else:
		if('pmic' in target or 'adc' in target):
			for i in range(len(file_content)):
				if(file_content[i]==[]):
					continue
				tar=file_content[i]['TARGET']
				if(tar==target):
					j=i-1
					while(j>=0 and 'dimm' not in file_content[j]['TARGET']):
						j-=1
					cmd='attributes translate'+" "+file_content[j]['TARGET']
					res=subprocess.Popen(cmd,shell=True,stdout=subprocess.PIPE)
					output,_=res.communicate()
					out=output.decode("utf8")
					x=out.strip("\n")
					x=x.split(" ")
					if('/' not in x[0]):
						x[0],x[-1]=x[-1],x[0]
					path_list[x[-1]]=x[0]
					print(json.dumps(path_list,indent=4))
		else:
			cmd='attributes translate'+" "+target
			res=subprocess.Popen(cmd,shell=True,stdout=subprocess.PIPE)
			output,_=res.communicate()
			out=output.decode("utf8")
			p=out.split(" ")[-1]
			x=p.rfind('/')
			p=p[:x]
			cmd='attributes translate'+" "+p
			res=subprocess.Popen(cmd,shell=True,stdout=subprocess.PIPE)
			output,_=res.communicate()
			out=output.decode("utf8")
			while('Failure'in out):
				x=p.rfind('/')
				p=p[:x]
				cmd='attributes translate'+" "+p
				res=subprocess.Popen(cmd,shell=True,stdout=subprocess.PIPE)
				output,_=res.communicate()
				out=output.decode("utf8")
			x=out.strip("\n")
			x=x.split(" ")
			if('/' not in x[0]):
				x[0],x[-1]=x[-1],x[0]
			path_list[x[-1]]=x[0]
			print(json.dumps(path_list,indent=4))
			exit()
def usage():
	'''This functions gives info about options we can specify in our command'''
	with open('help_usage.txt','r') as f:
		lines=f.readlines()
		for i in lines:
			print(i)
host_name=subprocess.run(['hostname'],stdout=subprocess.PIPE)
try:
	ind=sys.argv.index('-host')
	host=sys.argv[ind+1]
	del sys.argv[ind]
	del sys.argv[ind]
	try:
		ind=sys.argv.index('-password')
		pwd=sys.argv[ind+1]
		del sys.argv[ind]
		del sys.argv[ind]
	except:
		pwd='0penBmc0'
except:
	host='rain100bmc.aus.stglabs.ibm.com'
	pwd='0penBmc0'
	#print("Connecting to ",host)
try:
	ind=sys.argv.index('-user')
	uname=sys.argv[ind+1]
	del sys.argv[ind]
	del sys.argv[ind]
except:
	uname='service'
if('bmc' not in host_name.stdout.decode('utf-8')):
	import paramiko
	import scp
	ssh_client=paramiko.SSHClient()
	ssh_client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
	ssh_client.connect(host,port=22, username=uname, password=pwd)
	if(len(sys.argv)>1 and sys.argv[1] in ['import','export','read','write','translate']):
		cmd='sudo -i attributes '+" ".join(sys.argv[1:])
		stdin, stdout, stderr = ssh_client.exec_command(cmd)
		print(stdout.read().decode('utf-8'))
		exit()
	stdin, stdout, stderr = ssh_client.exec_command("sudo -i attributes export")
	file_content=stdout.read().decode('utf-8')
	file_content=file_content.split("target")
elif(len(sys.argv)>1 and sys.argv[1] in ['import','export','read','write','translate']):
	cmd='attributes '+" ".join(sys.argv[1:])
	res=subprocess.Popen(cmd,shell=True,stdout=subprocess.PIPE)
	output,_=res.communicate()
	out=output.decode("utf8")
	print(out)
	exit()
elif(sys.stdin.isatty()):
		res=subprocess.Popen(['attributes','export'], stdout=subprocess.PIPE)
		output, _ = res.communicate()
		out=output.decode("utf-8")
		file_content=out.split("target")
else:
        file_content=sys.stdin.read()
        file_content=file_content.split("target")
Nodes=OrderedDict()
t=''
file_content=processFile(file_content)#returns a list of dictionaries
flag=0
subdictionaries=OrderedDict()
l=[]
sub=OrderedDict()
sub1=OrderedDict()
s=[]
json_file=open('output.json','w')
if(len(sys.argv)>=2):
	if(sys.argv[1]=='-help'):
		sys.argv[1]='-h'
	elif(sys.argv[1]=='-target'):
		sys.argv[1]='-t'
	elif(sys.argv[1]=='-path'):
		sys.argv[1]='-p'
	elif(sys.argv[1]=='-list'):
		sys.argv[1]='-l'
	elif(sys.argv[1]=='-nodes'):
		sys.argv[1]='-n'
	elif(sys.argv[1]=='-json'):
		sys.argv[1]='-j'
	elif(sys.argv[1]=='-find'):
		sys.argv[1]='-f'
if(len(sys.argv)<2 or (len(sys.argv)>=2 and (sys.argv[1] not in ['-h','-t','-p','-l','-n','-f','-parent','-child']))):#Goes inside, if there is no option or the option is -j
	printTargets(file_content,0)
else:
	if(sys.argv[1]=='-h'):
		usage()
	elif(sys.argv[1]=='-parent'):
		if(len(sys.argv)==2):
			print("Enter target to get the parent")
		else:
			getParent(file_content,sys.argv[2])
	elif(sys.argv[1]=='-child'):
		if(len(sys.argv)==2):
			print("Enter target to get the children")
		else:
			getChild(file_content,sys.argv[2])
	else:
		printTargets(file_content,sys.argv[1])
		if(flag==0):
			invalidDetails(sys.argv[1])
json_file.close()

