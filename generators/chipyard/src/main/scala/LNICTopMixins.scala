package chipyard

import chisel3._

import freechips.rocketchip.config._
import freechips.rocketchip.diplomacy._
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
    vonly.nic_mac_addr := nicio.nic_mac_addr
    vonly.switch_mac_addr := nicio.switch_mac_addr
    vonly.nic_ip_addr := nicio.nic_ip_addr
    // connect L-NIC to tiles
    require(outer.lnicTiles.size == 1, "For now, L-NIC only supports single tile systems.")
    outer.lnicTiles.foreach { tile =>
      lnic.module.io.core.net_in <> tile.module.net.get.net_out
      tile.module.net.get.net_in <> lnic.module.io.core.net_out
      tile.module.net.get.meta_in := lnic.module.io.core.meta_out
    }
    nicio
  }

  // Connect L-NIC to simulated network.
  def connectSimNetwork(clock: Clock, reset: Bool) {
    val sim = Module(new SimNetwork)
    sim.io.clock := clock
    sim.io.reset := reset
    sim.io.net.out <> net.get.out
    net.get.in <> sim.io.net.in
    net.get.nic_mac_addr := sim.io.net.nic_mac_addr
    net.get.switch_mac_addr := sim.io.net.switch_mac_addr
    net.get.nic_ip_addr := sim.io.net.nic_ip_addr
  }

}

