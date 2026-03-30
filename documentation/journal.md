# Train Tracker journal
This is an engineering journal for me to document my progress. I don't know how much I will actually use this thing, but hopefully I stick to it.
## Mar 22, 2026
Today I started actually working on this project because I want to seriously commit to it. I have been thinking of making a transit tracker screen for a while because I think it would look cool and it sounds fun.

As of now, I have just gotten my API keys from the CTA webiste and I started this repo. I addedsome things to do in todo.md (although I should add some stuff about researching hardware to buy on there), and that's kind of all I have done for this today.

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

## mar. 24. 2026

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

