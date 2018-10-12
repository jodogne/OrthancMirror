#!/usr/bin/python
# -*- coding: utf-8 -*-

# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
# Copyright (C) 2017-2018 Osimis S.A., Belgium
#
# This program is free software: you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.



URL = 'http://127.0.0.1:8042'
TARGET = 'sample'


#
# This sample code shows how to setup a simple, high-performance DICOM
# auto-routing. All the DICOM instances that arrive inside Orthanc
# will be sent to a remote modality. A producer-consumer pattern is
# used. The target modality is specified by the TARGET variable above:
# It must match an entry in the Orthanc configuration file inside the
# "DicomModalities" section.
#
# NOTE: This sample only works with Orthanc >= 0.5.2. Make sure that
# Orthanc was built with "-DCMAKE_BUILD_TYPE=Release" to get the best
# performance.
#

import Queue
import sys
import time
import threading

import RestToolbox


#
# Queue that is shared between the producer and the consumer
# threads. It holds the instances that are still to be sent.
#

queue = Queue.Queue()


#
# The producer thread. It monitors the arrival of new instances into
# Orthanc, and pushes their ID into the shared queue. This code is
# based upon the "ChangesLoop.py" sample code.
#

def Producer(queue):
    current = 0

    while True:
        r = RestToolbox.DoGet(URL + '/changes', {
            'since' : current,
            'limit' : 4   # Retrieve at most 4 changes at once
            })

        for change in r['Changes']:
            # We are only interested interested in the arrival of new instances
            if change['ChangeType'] == 'NewInstance':
                queue.put(change['ID'])

        current = r['Last']

        if r['Done']:
            time.sleep(1)


#
# The consumer thread. It continuously reads the instances from the
# queue, and send them to the remote modality. Each time a packet of
# instances is sent, a single DICOM connexion is used, hence improving
# the performance.
#

def Consumer(queue):
    TIMEOUT = 0.1
    
    while True:
        instances = []

        while True:
            try:
                # Block for a while, waiting for the arrival of a new
                # instance
                instance = queue.get(True, TIMEOUT)

                # A new instance has arrived: Record its ID
                instances.append(instance)
                queue.task_done()

            except Queue.Empty:
                # Timeout: No more data was received
                break

        if len(instances) > 0:
            print('Sending a packet of %d instances' % len(instances))
            start = time.time()

            # Send all the instances with a single DICOM connexion
            RestToolbox.DoPost('%s/modalities/sample/store' % URL, instances)

            # Remove all the instances from Orthanc
            for instance in instances:
                RestToolbox.DoDelete('%s/instances/%s' % (URL, instance))

            # Clear the log of the exported instances (to prevent the
            # SQLite database from growing indefinitely). More simply,
            # you could also set the "LogExportedResources" option to
            # "false" in the configuration file since Orthanc 0.8.3.
            RestToolbox.DoDelete('%s/exports' % URL)

            end = time.time()
            print('The packet of %d instances has been sent in %d seconds' % (len(instances), end - start))


#
# Thread to display the progress
#

def PrintProgress(queue):
    while True:
        print('Current queue size: %d' % (queue.qsize()))
        time.sleep(1)


#
# Start the various threads
#

progress = threading.Thread(None, PrintProgress, None, (queue, ))
progress.daemon = True
progress.start()

producer = threading.Thread(None, Producer, None, (queue, ))
producer.daemon = True
producer.start()

consumer = threading.Thread(None, Consumer, None, (queue, ))
consumer.daemon = True
consumer.start()


#
# Active waiting for Ctrl-C
#

while True:
    time.sleep(0.1)
