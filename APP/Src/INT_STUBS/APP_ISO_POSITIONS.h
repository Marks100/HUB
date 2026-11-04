#ifndef APP_ISO_POSITIONS_H
#define APP_ISO_POSITIONS_H

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
/* The ISO positions an ERX can be at: five wheel positions on each of five axles. The ones used for the heartbeat
   (AUTOCFG_HEARTBEAT_BASE_ID plus the position), the diagnostic IDs (0x600/0x700 plus the position) and the
   HUB's own configuration (app_autocfg_init_s, INTEGRATION_STUBS.c) are these and no others.

   The tens digit is the axle, the units digit the wheel: 6 far left, 7 inner left, 8 centre line, 9 inner right,
   A far right. Axle 1 is the front (steer) axle, axle 2 the first drive (pusher) axle, axle 3 the second drive (tag)
   axle, axle 4 the first trailer (drop) axle, axle 5 the second trailer (rearmost) axle. "Outer" is the far wheel
   of an axle, left or right. */
#define APP_ISO_AXLE_1_OUTER_LEFT     ( 0x16u )
#define APP_ISO_AXLE_1_INNER_LEFT     ( 0x17u )
#define APP_ISO_AXLE_1_CENTRE         ( 0x18u )
#define APP_ISO_AXLE_1_INNER_RIGHT    ( 0x19u )
#define APP_ISO_AXLE_1_OUTER_RIGHT    ( 0x1Au )

#define APP_ISO_AXLE_2_OUTER_LEFT     ( 0x26u )
#define APP_ISO_AXLE_2_INNER_LEFT     ( 0x27u )
#define APP_ISO_AXLE_2_CENTRE         ( 0x28u )
#define APP_ISO_AXLE_2_INNER_RIGHT    ( 0x29u )
#define APP_ISO_AXLE_2_OUTER_RIGHT    ( 0x2Au )

#define APP_ISO_AXLE_3_OUTER_LEFT     ( 0x36u )
#define APP_ISO_AXLE_3_INNER_LEFT     ( 0x37u )
#define APP_ISO_AXLE_3_CENTRE         ( 0x38u )
#define APP_ISO_AXLE_3_INNER_RIGHT    ( 0x39u )
#define APP_ISO_AXLE_3_OUTER_RIGHT    ( 0x3Au )

#define APP_ISO_AXLE_4_OUTER_LEFT     ( 0x46u )
#define APP_ISO_AXLE_4_INNER_LEFT     ( 0x47u )
#define APP_ISO_AXLE_4_CENTRE         ( 0x48u )
#define APP_ISO_AXLE_4_INNER_RIGHT    ( 0x49u )
#define APP_ISO_AXLE_4_OUTER_RIGHT    ( 0x4Au )

#define APP_ISO_AXLE_5_OUTER_LEFT     ( 0x56u )
#define APP_ISO_AXLE_5_INNER_LEFT     ( 0x57u )
#define APP_ISO_AXLE_5_CENTRE         ( 0x58u )
#define APP_ISO_AXLE_5_INNER_RIGHT    ( 0x59u )
#define APP_ISO_AXLE_5_OUTER_RIGHT    ( 0x5Au )

/* The ends of the range: the lowest and the highest position, so the lowest and the highest heartbeat ID */
#define APP_ISO_POSITION_FIRST        ( APP_ISO_AXLE_1_OUTER_LEFT )
#define APP_ISO_POSITION_LAST         ( APP_ISO_AXLE_5_OUTER_RIGHT )

#endif /* APP_ISO_POSITIONS_H */

/****************************** END OF FILE *******************************************************/