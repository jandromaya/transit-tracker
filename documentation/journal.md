# Train Tracker journal
This is an engineering journal for me to document my progress. I don't know how much I will actually use this thing, but hopefully I stick to it.
## Mar 22, 2026
Today I started actually working on this project because I want to seriously commit to it. I have been thinking of making a transit tracker screen for a while because I think it would look cool and it sounds fun.

As of now, I have just gotten my API keys from the CTA website and I started this repo. I added some things to do in todo.md (although I should add some stuff about researching hardware to buy on there), and that's kind of all I have done for this today.

This is my overall game plan:
- Read API docs to get a rough idea of how the API works
- Make a prototype program that just prints bus/train info to the command line from an ESP32
- ^ while I build this, order some hardware (preferably one of those large LED matrix screens, as that's kind of like what the CTA uses)
- Once I get the prototype built okay, try connecting it to the LED matrix screen to display output on there
- Once I am happy with the output on the LED matrix screen, design a PCB for the circuit
- test PCB
- once I am satisfied with PCB, make an enclosure for whole thing

Excited!

## Mar 23, 2026
Okay, today I began looking at LED matrix options. This one looks good:
[Adafruit](https://www.adafruit.com/product/5036)
[AliExpress](https://www.aliexpress.us/item/2255799816372142.html?aff_fcid=e5227b8c1e8842269d58a8cb797e9c98-1774266289656-03046-_c2JPsqBp&tt=CPS_NORMAL&aff_fsk=_c2JPsqBp&aff_platform=portals-tool&sk=_c2JPsqBp&aff_trace_key=e5227b8c1e8842269d58a8cb797e9c98-1774266289656-03046-_c2JPsqBp&terminal_id=8931f672053841ee8a5de036aeb01fa5&afSmartRedirect=y&gatewayAdapt=glo2usa)
[Waveshare](https://www.waveshare.com/rgb-matrix-p2.5-64x32.htm?sku=23707)
It has its own library that I can use to control the matrix, and you can link multiple together. The linking together seems useful, so I *may* want to do it, but I think I could maybe get away with just using one of them. I guess the design trade off would be that I can show more information with two matrices linked together (i.e., stop names), but since I know exactly what stops are near my house for what routes, I don't know if I *need* to link two of these.

To connect the matrix to a board, I need a 16-pin IDC connector, like this one:
[DigiKey](https://www.digikey.com/en/products/detail/molex/0702461601/760169?gclsrc=aw.ds&gad_source=1&gad_campaignid=20560900243&gbraid=0AAAAADrbLliJBfFHop-t9JOzA3-XaYj9B&gclid=CjwKCAjwyYPOBhBxEiwAgpT8Py8OolFclUmhsOUIjvch43n7zdOdmsElHDj1cdLqBWji2bsKEcZodxoCDzMQAvD_BwE)

To power the board, Adafruit suggests using using their [wall adapter](https://www.adafruit.com/product/1466) and [2.1mm jack](https://www.adafruit.com/product/368).

This [other transit tracker project](https://transit-tracker.eastsideurbanism.org/docs/build-guide/materials#note-about-displays) uses just a [USB power supply](https://www.adafruit.com/product/1994) and a [USB-C cable](https://www.adafruit.com/product/5031).

So all together I think that's all the hardware that I need  to connect the matrix to an ESP32.

---

Ok that was this morning, tonight I set up the file structure for esp idf and I will read some of the api docs before I go to bed. 

## Mar. 24. 2026

Today I:

- went through and annotated the bus time API, gonna read the other one later (maybe omw to work)
- Went through the train time API (I did do it omw to work and also before bed lol)

As far as the bus time API goes, it looks like the main thing to worry about is just "predictions" and probably delays/cancellations (which are part of the dynamic action types). I highilighted the relevant fields of the output that I need to worry about in my local copy of the document. I'll need cJSON.

for the traintime API, it looks like the "Arrivals API" from page 5 will be the most useful. Appendix D also has useful information on how to turn this output into "minutes until this train arrives".

## Mar. 26, 2026

Yesteday and today I started coding up some basic programs, just getting the ESP32 connected to Wi-Fi and sending https requests and messing around with different things along the way. 

At first I was messing around with FreeRTOS tasks and semaphores, then I was using task notifications. Turns out I don't think I'll really need any of that, but it's in the code at the moment because that's just what I was trying out.

Right now, the ESP32 can get predictions of routes. From here, I need to parse the JSON responses so that the output is actually usable. I also will need to get multiple routes at once (which I'm pretty sure I can just do with the API).

## Mar. 30, 2026

I am travelling right now but I have continued working on this whenever possible. Here are a few design things I have been thinking about:

### Design thoughts

**General flow of the program**

- 30 second cycles
- 24 seconds
  - Output routes 
  - This seems kind of long, but depending on the number of routes you are retrieving you may have to scroll to get through all of them
    - Could be three screens of routes, each shown for 8 seconds for example
- 3 seconds
  - Output current time
    - Can just get this from CTA API or with SNTP
  - Could probably be taken out, but I just think it would be useful
- 3 seconds
  - Output other info
  - Maybe just "CTA tracker" or if there are different modes, maybe just the name of the mode

### Considerations/Edge cases

- Outdated predictions
  - Sometimes prediction timestamps are outdated (i.e., don't match the current time in the CTA system)
  - May want to check that the predictions have a timestamp within a threshold amount of time from the current time
- May want to consider dynamically allocating memory in perform_get_request()
  - if so, make it caller-owned
- What priority should the parsing task have?

## Mar. 31, 2026

Worked on this a little bit today. I updated the parse task to handle http responses from different sources, and right now it handles data from the bus prediction task and the get time task, printing the received data to the console with printf.

In the future, I want to mainly do two things:

1. Add the train data
2. Add some filtering of what to show on the display (for example, if a response's timestamp is too old, it should not show)

Once this is all handled, the minimum API side of the project is pretty close to being done I think. For a basic proof of concept, all I need to do now would be connecting all this stuff to an external display and printing

## Apr. 1, 2026

Okay, so I have been thinking a bit more about what I need hardware-wise, as I realized my earlier parts list is not great. Adafruit offers [this breakout board](https://learn.adafruit.com/adafruit-matrixportal-s3/pinouts) for ESP32 matrix control.  The breakout board itself is out of stock, but I can take the components and make my own thing. Here's what I need (based on what they have on this board):

| **Part**                     | **Quantity** | Unit **Price** | **Total Price** | **MPN**               | **Link**                                                     |
| ---------------------------- | ------------ | -------------- | --------------- | --------------------- | ------------------------------------------------------------ |
| LED Matrix                   | 2            | 17.99          |                 | RGB-Matrix-P2.5-64x32 | [Waveshare](https://www.waveshare.com/rgb-matrix-p2.5-64x32.htm?sku=23707) |
| HUB-75 Socket (maybe a 2x10) | 1            | 1.09           |                 | S6106-ND              | [DigiKey](https://www.digikey.com/en/products/detail/sullins-connector-solutions/PPPC102LFBN-RC/807245) |
| Level shifter                | 2            | 0.95           |                 | ahct245               | [DigiKey](https://www.digikey.com/en/products/detail/texas-instruments/SN74AHCT245N/277122?s=N4IgjCBcoGwJxVAYygMwIYBsDOBTANCAPZQDaIALGGABxwDsIAuoQA4AuUIAyuwE4BLAHYBzEAF9xhAExkQ6ABZJ20igFZmhGIhACAJlwC0YAAyy2nSCBCF2AT1a4ue7CklA) |
| USB-C Power Supply           | 1            | 7.95           |                 | 1994                  | [Adafruit](https://www.adafruit.com/product/1994)            |
| USB-C cable                  | 1            | 5.25           |                 | 5031                  | [DigiKey](https://www.digikey.com/en/products/detail/qualtek/3021107-01M/13181649?gclsrc=aw.ds&gad_source=1&gad_campaignid=20232005509&gbraid=0AAAAADrbLljWyom9jD6fqw8Tg_-qsQaIl&gclid=Cj0KCQjws83OBhD4ARIsACblj18_hTfEJbudFK00JUDDqoENaiXXbeZN6epAEKazdsYqEp8OduHiS_EaAhE2EALw_wcB) |
| 2x8 IDC Cable                | 1            | 1.95           |                 | 4170                  | [Adafruit](https://www.adafruit.com/product/4170?srsltid=AfmBOorfjdOh5D1p04Qbc45afL_Oq-bk-VF9IhERsBwBOS83qW3fk6iHNpM) |
| HUB-75 Plug                  | 1            | 0.32           |                 | S9171-ND              | [DigiKey](https://www.digikey.com/en/products/detail/sullins-connector-solutions/SBH11-PBPC-D08-ST-BK/1990064?_gl=1*j1ohis*_up*MQ..*_gs*MQ..&gclid=Cj0KCQjws83OBhD4ARIsACblj1-lxpeixAYIAsuX1di81NiLDhc_4hT3QPhrwWIrpjFBDfc82UlnVEAaAoDSEALw_wcB&gclsrc=aw.ds&gbraid=0AAAAADrbLlg19u8zw4DT2EaTiBKWrGHyu) |

Some thoughts:

- Power
  - On the breakout board, they power the matrix with the USB-C port
    - The USB-C port connects to two M3-threaded screw terminals
  - ESP32 seems to be powered with either JST connector or USB-C 
- HUB-75 connector
  - On the breakout board i put up there, they use a 2x10 instead of a 2x8. not totally sure why, but maybe it's helpful
  - Also, you could either have a socket here (i.e., a female connector) or a plug
    - If you have the plug, then you need to buy an IDC cable to connect to the matrix
- Multiplexer type thing
  - AHCT245
  - These were included in the adafruit breakout board I found
  - What are they?
    - **8-bit Octal Bus Transceiver**
  - Why do they use it?
    - Level shifting
    - Basically, the ESP32 is going to output at 3.3V, but the matrix needs input at 5V. This specific device helps bridge that gap

## apr. 6, 2026

Today I filled in the table above and bought everything. Exciting!

^ that was in the morning.

I added some (very basic) train API functionality. It's basically just performing a get request and outputting the response. Some next steps would be to format the reponse a little more (i.e., in countdown format like the buses). I also need to add my own logic for delays/DUE notifications because the train API doesn't do this automatically like the bus one.

I was also thinking... It may be better to delay the different tasks relative to each other rather than having the big offset at the beginning I currently have. Think about this more.

Regardless, I think my task scheduling may be a little messed up. The train task doesn't seem to be executing as often as I intended. Look into this also.

## Apr. 7, 2026

I woke up a little bit ago and started working on this. I fixed the task scheduling issue I noted above (the train task said`vTaskDelay(10000)` instead of `vTaskDelay(pdMS_TO_TICKS(10000))`).

I began formatting the train API responses a little bit by making the `print_train_info` helper function. Right now, the function outputs if a train is due or if there is a delay. I still need to figure out how to format the string predictions into a countdown. 

Here are some solutions:

- could I change the strings into unix time and compare them that way? this seems like easiest option
  - can use strptime to convert the string to a tm struct, then use mktime to convert the tm struct to unix time
- I could compare each part of the string to see what is different, but this seems overly complicated

Here are some edge cases to think about:

- what happens if current time is before midnight and next train comes after midnight
- what happens if time difference is negative (I think this shouldn't happen, but I didn't read any guarantees of that in the API guide)

## Apr. 8, 2026

I was going to work on the stuff above, but I realized that I could simplify what I already have a little bit.

I realized that right now I have three tasks that basically do the same thing because I was just coding and trying stuff as I went along. Instead of doing that, I think it may be simpler and more useful to have a generic get request task (`vPerformGetRequestTask`) and a separate request scheduler task (`vRequestSchedulerTask`. 

`vRequestSchedulerTask` could send `RequestType_t`  variables to the `vPerformGetRequestTask` using a queue. Based on the request type, `vRequestSchedulerTask` can use different URLs to make the correct GET request.

One more thought: how do you get the URL?

- Could be handled by the `vPerformGetRequestTask`
  - Would mean having some sort of if statement to decide the type of request received
  - feels more messy
- Could be handled by `vRequestSchedulerTask`
  - Would pass the URL on the queue, can be done with existing QueueData_t struct

I think I'll go with second option

## Apr. 9, 2026

Okay today I'm actually gonna work on the train formatting. Here is the output of the API:
2026-04-09T07:32:07

I need to turn this into unix time first and then compare the two values. Here's how to use [strptime()](https://pubs.opengroup.org/onlinepubs/007904875/functions/strptime.html).

%Y-%m-%dT%T

I did it! 

