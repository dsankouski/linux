import java.math.BigDecimal
import java.math.MathContext
//
//println(calculateRate(26000000L, 207,	3,	0, 0, 16))
//println(calculateRate(26000000L, 198,	3,	0, 0, 16))
//println(calculateRate(26000000L, 296,	5,	0, 0, 16))
//println(calculateRate(26000000L, 156,	3,	0, 0, 16))
//println(calculateRate(26000000L, 132,	3,	0, 0, 16))
//println(calculateRate(26000000L, 117,	3,	0, 0, 16))
//println(calculateRate(26000000L, 195,	3,	1, 0, 16))
//println(calculateRate(26000000L, 156,	3,	1, 0, 16))
//println(calculateRate(26000000L, 126,	3,	1, 0, 16))
//println(calculateRate(26000000L, 259,	4,	2, 0, 16))
//println(calculateRate(26000000L, 132,	3,	2, 0, 16))
//println(calculateRate(26000000L, 192,	3,	3, 0, 16))
// what's the formula?

//println(calculateRate(26000000L,30,	1,	2,	16213 , 0))
//println(calculateRate(26000000L,30,	1,	3,	16213 , 0))
//println(calculateRate(26000000L,30,	1,	4,	16213 , 0))
//println(calculateRate(26000000L, , 0, 0))
//println(calculateRate(26000000L, , 0, 0))
//println(calculateRate(26000000L, , 0, 0))

println(calculateFin(196608000L, 30,	1,	2,	16213 , 0))
println(calculateFin(1794000000L, 207,	3,	0, 0, 0))

fun calculateRate(fin: Long, m: Int, p: Int, s: Int, k: Int, ks: Int): BigDecimal {
    var result: BigDecimal
    val TWO = BigDecimal(2)

    var _fin: BigDecimal = BigDecimal(fin)
    var _m: BigDecimal = BigDecimal(m)
    var _k: BigDecimal = BigDecimal(k)
    var _s: BigDecimal = BigDecimal(s)
    var _p: BigDecimal = BigDecimal(p)
    var _ks: BigDecimal = BigDecimal(ks)

    var twoPowKs = TWO.pow(_ks.toInt())


    result = _fin
            .multiply(
                    _m.multiply(twoPowKs).plus(_k)
            )
            .divide(
                    _p.multiply(twoPowKs).multiply(TWO.pow(s)), MathContext.DECIMAL128
            )

    return result
}

fun calculateFin(rate: Long, m: Int, p: Int, s: Int, k: Int, ks: Int): BigDecimal {
    var result: BigDecimal
    val TWO = BigDecimal(2)

    var _rate: BigDecimal = BigDecimal(rate)
    var _m: BigDecimal = BigDecimal(m)
    var _k: BigDecimal = BigDecimal(k)
    var _s: BigDecimal = BigDecimal(s)
    var _p: BigDecimal = BigDecimal(p)
    var _ks: BigDecimal = BigDecimal(ks)

    var twoPowKs = TWO.pow(_ks.toInt())


    result = _rate
            .multiply(
                    _p.multiply(twoPowKs).multiply(TWO.pow(s))
            )
            .divide(
                    _m.multiply(twoPowKs).plus(_k), MathContext.DECIMAL128
            )

    return result
}

