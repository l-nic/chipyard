package chipyard

import chisel3._

import freechips.rocketchip.config._
import freechips.rocketchip.diplomacy._
import icenet.NICIOvonly
import lnic._

/** Top-level mixins for including LNIC **/

trait CanHaveLNIC { this: Subsystem =>
  val lnicTiles = tiles
  val lnicOpt = p(LNICKey).map { params =>
    val lnic = LazyModule(new LNIC)
    lnic
  }
}

trait CanHaveLNICModuleImp extends LazyModuleImp {
  val outer: CanHaveLNIC

  val net = outer.lnicOpt.map { lnic =>
    // create and connect to top-level IO
    val nicio = IO(new NICIOvonly)
    val vonly = NICIOvonly(lnic.module.io.net)
    nicio.out <> vonly.out
    vonly.in <> nicio.in
    vonly.macAddr := nicio.macAddr
    vonly.rlimit := nicio.rlimit
    vonly.pauser := nicio.pauser
    // connect L-NIC to tiles
    val num_cores = outer.lnicTiles.size
    for (i <- 0 until num_cores) {
      val tile = outer.lnicTiles(i)
      lnic.module.io.core(i).net_in <> tile.module.net.get.net_out
      lnic.module.io.core(i).add_context := tile.module.net.get.add_context
      lnic.module.io.core(i).get_next_msg := tile.module.net.get.get_next_msg
      tile.module.net.get.net_in <> lnic.module.io.core(i).net_out
      tile.module.net.get.meta_in := lnic.module.io.core(i).meta_out
    }
    nicio
  }

  // Connect L-NIC to simulated network.
  def connectSimNetwork(clock: Clock, reset: Bool) {
    val sim = Module(new SimNetwork)
    val latency = Module(new LatencyModule)
    sim.io.clock := clock
    sim.io.reset := reset

    sim.io.net.out <> latency.io.net.out
    latency.io.net.in <> sim.io.net.in

    latency.io.nic.in <> net.get.out
    net.get.in <> latency.io.nic.out

    net.get.macAddr := sim.io.net.macAddr
    net.get.rlimit := sim.io.net.rlimit
    net.get.pauser := sim.io.net.pauser
  }

}

