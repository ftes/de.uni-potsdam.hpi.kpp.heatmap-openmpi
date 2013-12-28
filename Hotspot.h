/* 
 * File:   Hotspot.h
 * Author: fredrik
 *
 * Created on November 14, 2013, 4:31 PM
 */

#ifndef HOTSPOT_H
#define	HOTSPOT_H

class Hotspot {
public:
    Hotspot(int x, int y, int startRound, int endRound);
    int startRound;
    int endRound;
    int x;
    int y;
private:
};

#endif	/* HOTSPOT_H */

