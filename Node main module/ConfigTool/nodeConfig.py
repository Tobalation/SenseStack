#   Copyright 2014 Dan Krause
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import socket
import http.client
import io
import xml.etree.ElementTree as ET
import requests
import json
from PyInquirer import prompt, print_json, Validator, ValidationError
import re
import six
from pyfiglet import figlet_format
import webbrowser

try:
    from termcolor import colored
except ImportError:
    colored = None



class SSDPResponse(object):
    class _FakeSocket(io.BytesIO):
        def makefile(self, *args, **kw):
            return self
    def __init__(self, response):
        r = http.client.HTTPResponse(self._FakeSocket(response))
        r.begin()
        self.location = r.getheader("location")
        self.usn = r.getheader("usn")
        self.st = r.getheader("st")
        self.cache = r.getheader("cache-control").split("=")[1]
    def __repr__(self):
        return "<SSDPResponse({location}, {st}, {usn})>".format(**self.__dict__)

class IPAddressValidator(Validator):
    def validate(self, document):

        pattern = re.compile(r"""
            ^
            (?:
            # Dotted variants:
            (?:
                # Decimal 1-255 (no leading 0's)
                [3-9]\d?|2(?:5[0-5]|[0-4]?\d)?|1\d{0,2}
            |
                0x0*[0-9a-f]{1,2}  # Hexadecimal 0x0 - 0xFF (possible leading 0's)
            |
                0+[1-3]?[0-7]{0,2} # Octal 0 - 0377 (possible leading 0's)
            )
            (?:                  # Repeat 0-3 times, separated by a dot
                \.
                (?:
                [3-9]\d?|2(?:5[0-5]|[0-4]?\d)?|1\d{0,2}
                |
                0x0*[0-9a-f]{1,2}
                |
                0+[1-3]?[0-7]{0,2}
                )
            ){0,3}
            |
            0x0*[0-9a-f]{1,8}    # Hexadecimal notation, 0x0 - 0xffffffff
            |
            0+[0-3]?[0-7]{0,10}  # Octal notation, 0 - 037777777777
            |
            # Decimal notation, 1-4294967295:
            429496729[0-5]|42949672[0-8]\d|4294967[01]\d\d|429496[0-6]\d{3}|
            42949[0-5]\d{4}|4294[0-8]\d{5}|429[0-3]\d{6}|42[0-8]\d{7}|
            4[01]\d{8}|[1-3]\d{0,9}|[4-9]\d{0,8}
            )
            $
        """, re.VERBOSE | re.IGNORECASE)
        if(pattern.match(document.text) is  None):
            raise ValidationError(
                    message='Please enter a valid IPv4 address',
                    cursor_position=len(document.text))  # Move cursor to end
            
            


class SenseStackManager:
        
    def _discoverUPnP(self, service="upnp:rootdevice", timeout=5, retries=1, mx=3):
        group = ("239.255.255.250", 1900)
        message = "\r\n".join([
            'M-SEARCH * HTTP/1.1',
            'HOST: {0}:{1}',
            'MAN: "ssdp:discover"',
            'ST: {st}','MX: {mx}','',''])
        socket.setdefaulttimeout(timeout)
        responses = {}
        for _ in range(retries):
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
            # sock.sendto(message.format(*group, st=service, mx=mx), group)
            message_bytes = message.format(*group, st=service, mx=mx).encode('utf-8')
            sock.sendto(message_bytes, group)
            while True:
                try:
                    response = SSDPResponse(sock.recv(1024))
                    responses[response.location] = response
                except ( socket.timeout, http.client.RemoteDisconnected) as e:
                    break
        return list(responses.values())
    
    def discoverSenseStack(self):
        SSDPResponses = self._discoverUPnP()
        discoveredNodes = {}

        for response in SSDPResponses:
            deviceDescriptionURL = response.location
            r = requests.get(deviceDescriptionURL)
            if (r.status_code != 200):
                continue

            responseXml = ET.fromstring(r.content)
            try:
                manufacturer = responseXml.find("{urn:schemas-upnp-org:device-1-0}device").find("{urn:schemas-upnp-org:device-1-0}manufacturer").text
            except AttributeError:
                continue

            if (manufacturer != "SenseStack"):
                continue

            friendlyName = responseXml.find("{urn:schemas-upnp-org:device-1-0}device").find("{urn:schemas-upnp-org:device-1-0}friendlyName").text
            discoveredNodes[deviceDescriptionURL[:deviceDescriptionURL.find(':',9)]] = friendlyName

        return discoveredNodes

    def getNodeInfo(self, nodeIP):
        r = requests.get(nodeIP + "/getNodeInfo")
        if (r.status_code != 200):
            return None
        return json.loads(r.content)

manager = SenseStackManager()
# discoveredNodes =  manager.discoverSenseStack()
# for nodeIP in discoveredNodes.keys():
#     print(manager.getNodeInfo(nodeIP))

class SenseStackManagerCLI:
    
    prompt_findNodeMethod = [
        {
            'type': 'list',
            'name': 'choice',
            'message': 'How do you want to find node?',
            'choices': ["Auto scan", "Manual input IP address"]
        }
    ]
    prompt_enterIP = [{'type': 'input', 'name':"ipInput", 'message':"Please enter node IP address", 'validate': IPAddressValidator}]
    prompt_selectOption = [
         {
            'type': 'list',
            'name': 'choice',
            'message': 'Node option',
            'choices': ["View status", "Go to config page"]
        }
    ]

    prompt_selectNode = [
        {
            'type': 'list',
            'name': 'choice',
            'message': 'Please select SenseStack that you want to config',
            'choices': []
        }
    ]


    nodes = {}
    selectedIP = ""


    def __init__(self):
        self.manager = SenseStackManager()
        self.prettyPrint("SenseStack",color="blue", figlet=True)
        self.prettyPrint("Welcome to SenseStack CLI", "green")
        self.start()
        
        super().__init__()
    
    def start(self):
        if (1):
            if(self.findNode()):
                if(self.selectNode()):
                    if(self.selectOption()):
                        pass


    def findNode(self):


        findNodeMethod_ans = prompt(self.prompt_findNodeMethod)
        if (findNodeMethod_ans.get("choice") == "Auto scan"):
            self.nodes = manager.discoverSenseStack()

        if (findNodeMethod_ans.get("choice")=="Manual input IP address"):
            nodeIPaddress_ans = prompt(self.prompt_enterIP)
            ip = "http://" + nodeIPaddress_ans.get("ipInput") 
            try:
                self.nodes[ip] = manager.getNodeInfo(ip)["name"]
            except:
                pass
            
        if (len(self.nodes) == 0):
            self.prettyPrint("No node found","red")
            return False
        print("\nIP address\t\t\tNode name")
        print("=======================================================")


        for key in self.nodes:
            print(f"{key[7:]}\t\t\t{self.nodes.get(key)}")
        
        print("")

        return True

    def selectOption(self):
        findNodeMethod_ans = prompt(self.prompt_selectOption)
        if (findNodeMethod_ans.get("choice") == "View status"):
            self.printNodeStatus(self.selectedIP)
        if (findNodeMethod_ans.get("choice") == "Go to config page"):
            webbrowser.open(self.selectedIP+"/status")

    
    def selectNode(self):
        self.prompt_selectNode[0]["choices"] = []
        for key in self.nodes:
            self.prompt_selectNode[0]["choices"].append(self.nodes.get(key) + "       " + key[7:])

        selectNode_ans = prompt(self.prompt_selectNode)
        self.selectedIP = "http://" + selectNode_ans.get("choice").split('       ')[1]
        return True




    def printNodeStatus(self, ip):
        r = requests.get(ip+"/getNodeInfo")
        nodeStatus = r.json()
        print("===== Node status =====")
        print("name:\t\t" + nodeStatus["name"])
        print("UUID:\t\t" + nodeStatus["uuid"])
        print("LAT:\t\t" + str(nodeStatus["lat"]))
        print("LNG:\t\t" + str(nodeStatus["long"]))
        print("Endpoint:\t" + nodeStatus["currentEndpoint"])
        print("Token:\t\t" + nodeStatus["currentToken"][:5] + "......" + nodeStatus["currentToken"][-5:] )
        print("Latest post reply:\t" + nodeStatus["latestPostReply"])
        print("Update interval:\t" + str(nodeStatus["updateInterval"]))
        print("Uptime (s):\t" + str(nodeStatus["uptime"]))
        print("Connnected sensors:\t" + str(nodeStatus["connectedSensors"]))

        # print(r.content)
        



    def prettyPrint(self, string, color, font="slant", figlet=False):
        if colored:
            if not figlet:
                six.print_(colored(string, color))
            else:
                six.print_(colored(figlet_format(
                    string, font=font), color))
        else:
            six.print_(string)


cli = SenseStackManagerCLI()
cli.start()